#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/select.h>

#include <linux/videodev2.h>
#include <cwchar>
#include <QChar>
#include "rds_decoder.h"

char RadioText [2][65]           =   {0,};
bool NewRadioText[2]={false,false};
uint8_t ActivePage = 0;

char StationText [65]         =   {0,};
bool NewStationText=false;
uint16_t CurrentPICode  =   0;

int fdradiodev=-1;
bool restart_decoding = false;
bool active = false;
bool rdsfound = false;

rds_decoder::rds_decoder(int fd)
{
    fdradiodev = fd;
    restart_decoding = false;
    active = true;
}

void rds_decoder::run(){
    decode_data();
}

void rds_decoder::reset(){
    bzero(RadioText,sizeof(RadioText));
    bzero(StationText, sizeof(StationText));
    rdsfound=false;

}

void rds_decoder::end(){
    active=false;
}

void rds_decoder::decode_data(){
    int n_read=0;
    int rds_read_timout_cnt=0;
    fd_set set;
    struct timeval timeout;
    int rv;
    uint8_t rds_data_buf[3];
    uint16_t Group[4];
    RECEPTION_STATE BlockState=rds_decoder::GET_BLOCK_A;

    while( active == true){
            /* We need Block A - B - C - D in this order to process datat */
             FD_ZERO(&set); /* clear the set */
             FD_SET(fdradiodev, &set); /* add our file descriptor to the set */

             timeout.tv_sec = 1;
             timeout.tv_usec = 0;

             rv = select(fdradiodev + 1, &set, NULL, NULL, &timeout);
              bzero(rds_data_buf, sizeof(rds_data_buf));
              if(rv == -1){
                /* an error accured */
                  n_read=0;
                  rds_read_timout_cnt++;
                  if(rds_read_timout_cnt>10){
                     bzero(RadioText, sizeof(RadioText));
                     bzero(StationText, sizeof(StationText));
                     CurrentPICode = 0;
                     rdsfound=false;
                  }
              } else if(rv == 0) {
                 n_read=0; /* a timeout occured */
                 rds_read_timout_cnt++;
                 if(rds_read_timout_cnt>10){
                    bzero(RadioText, sizeof(RadioText));
                    bzero(StationText, sizeof(StationText));
                    CurrentPICode = 0;
                    rdsfound=false;
                 }
              } else {
                    n_read = read(fdradiodev, rds_data_buf, sizeof(rds_data_buf));
                    rdsfound=true;
                    rds_read_timout_cnt=0;
              }

             if(n_read > 0){

                v4l2_rds_data* rds_ptr = (v4l2_rds_data*)&rds_data_buf[0]; /* With the current hardware this in non blocking */
                emit NewRDSMessage(rds_ptr->block,rds_ptr->lsb,rds_ptr->msb);


                if(rds_ptr->block&V4L2_RDS_BLOCK_ERROR){
                }

                if(rds_ptr->block&V4L2_RDS_BLOCK_CORRECTED){
                }


                switch( rds_ptr->block & V4L2_RDS_BLOCK_MSK ){
                    case V4L2_RDS_BLOCK_A:{

                        if(BlockState != GET_BLOCK_A){
                            BlockState = GET_BLOCK_B;

                        } else {
                            BlockState = GET_BLOCK_B;
                            Group[0]=( rds_ptr->lsb) | ( rds_ptr->msb << 8);
                        }

                    } break;

                    case V4L2_RDS_BLOCK_B:{

                        if(BlockState != GET_BLOCK_B){
                             BlockState = GET_BLOCK_A;
                        } else {
                            BlockState = GET_BLOCK_C;
                            Group[1]=( rds_ptr->lsb) | ( rds_ptr->msb << 8);
                        }
                    } break;

                    case V4L2_RDS_BLOCK_C:{

                        if(BlockState != GET_BLOCK_C){
                             BlockState = GET_BLOCK_A;
                        } else {
                            BlockState = GET_BLOCK_D;
                            Group[2]=( rds_ptr->lsb) | ( rds_ptr->msb << 8);
                        }
                    } break;

                    case V4L2_RDS_BLOCK_C_ALT:{

                        if(BlockState != GET_BLOCK_C){
                             BlockState = GET_BLOCK_A;
                        } else {
                            BlockState = GET_BLOCK_C;
                        }
                    } break;

                    case V4L2_RDS_BLOCK_D:{

                       if(BlockState != GET_BLOCK_D){
                            BlockState = GET_BLOCK_A;
                       } else {
                          Group[3]=( rds_ptr->lsb) | ( rds_ptr->msb << 8);
                          decode_group(Group);
                          BlockState = GET_BLOCK_A;
                       }
                    } break;



                    case V4L2_RDS_BLOCK_INVALID:{

                    }break;

                    default: {
                       fprintf(stderr, "?:" );
                    } break;

             }
         } else {
            QThread::msleep(25);
         }
    }


}

void rds_decoder::decode_group(uint16_t* Group){

    emit NewRDSGroup(Group[0],Group[1],Group[2],Group[3]);
    /* First word is Prog ID */
    uint16_t PI_Code = Group[0];
    /* Second Block is group type code + Version A or B + Traffic Prog Code + Prog Type Code */
    uint8_t GroupTypeCode   = (Group[1]&0xF000)>>12;
    /* uint8_t Version         = (Group[1]&0x0800) >> 11; */

    uint16_t Block1 = Group[1];
    uint16_t TypeB_Flag = (Block1&0x0800);
    uint8_t A_B_Flag=0;

    uint8_t Offset          = 0;

    /* We only decode group 2a/b and 0a/b */
    if(PI_Code != CurrentPICode ){
        /* New Station found */
        bzero(RadioText, sizeof(RadioText));
        bzero(StationText, sizeof(StationText));
        CurrentPICode = PI_Code;
    }


    switch(GroupTypeCode){

        case 0:{
            Offset =(Group[1]&0x0003)*2;
            /*
              This is for the Stationtext
              We assume that in the memory is 0x00 or cahrs,
              if they differ we do a clean and start updating */

            if((int16_t)(Offset+1) < sizeof(StationText) ){
                if(StationText[Offset]!=0x00){
                    if(StationText[Offset] != ( (Group[3]&0xFF00)>>8 ) ){
                        bzero(StationText, sizeof(StationText));
                        NewStationText = true;
                    }
                    StationText[Offset]=(Group[3]&0xFF00)>>8;
                } else {
                    StationText[Offset]=(Group[3]&0xFF00)>>8;
                }


                if(StationText[Offset+1]!=0x00){
                    if(StationText[Offset+1] != ( (Group[3]&0x00FF) ) ){
                        bzero(StationText, sizeof(StationText));
                        NewStationText = true;
                    }
                }
                StationText[Offset+1]=(Group[3]&0x00FF);

            }
            /* Ignore everything besides the boundarys */





        } break;

        case 2:{

            if(Block1&0x0010){
              /* B Flag */
              A_B_Flag = 1;

            } else {
              A_B_Flag = 0;

            }

            if( TypeB_Flag != 0){
                Offset = (Group[1]&0x000F)*2;
                if((int16_t)(Offset+1) < sizeof(RadioText)){
                    if( RadioText[A_B_Flag][Offset] != 0x00){
                        if( RadioText[A_B_Flag][Offset] != ((Group[3]&0xFF00)>>8) ){
                             bzero(RadioText[A_B_Flag], sizeof(RadioText[0]));
                             NewRadioText[A_B_Flag] = true;
                        }
                    }
                    RadioText[A_B_Flag][Offset]=(Group[3]&0xFF00)>>8;


                    if( RadioText[A_B_Flag][Offset+1] != 0x00){
                        if( RadioText[A_B_Flag][Offset+1] != (Group[3]&0x00FF) ){
                            bzero(RadioText[A_B_Flag], sizeof(RadioText[0]));
                            NewRadioText[A_B_Flag] = true;
                        }
                    }
                    RadioText[A_B_Flag][Offset+1]=(Group[3]&0x00FF);
                }

            } else {
                Offset = (Group[1]&0x000F)*4;
                if((int16_t)(Offset+4) < sizeof(RadioText)){

                    if( RadioText[A_B_Flag][Offset]!=0x00){
                        if(RadioText[A_B_Flag][Offset] != ( ( Group[2]&0xFF00)>>8 ) ){
                            bzero(RadioText[A_B_Flag], sizeof(RadioText[0]));
                            NewRadioText[A_B_Flag] = true;
                        }
                    }
                    RadioText[A_B_Flag][Offset]=(Group[2]&0xFF00)>>8;

                    if( RadioText[A_B_Flag][Offset+1]!=0x00){
                        if(RadioText[A_B_Flag][Offset+1] != ( Group[2]&0x00FF ) ){
                            bzero(RadioText[A_B_Flag], sizeof(RadioText[0]));
                            NewRadioText[A_B_Flag] = true;
                        }
                    }
                    RadioText[A_B_Flag][Offset+1]=(Group[2]&0x00FF);


                    if( RadioText[A_B_Flag][Offset+2]!=0x00){
                        if(RadioText[A_B_Flag][Offset+2] != ( (Group[3]&0xFF00)>>8 ) ){
                            bzero(RadioText[A_B_Flag], sizeof(RadioText[0]));
                            NewRadioText[A_B_Flag] = true;
                        }
                    }
                    RadioText[A_B_Flag][Offset+2]=(Group[3]&0xFF00)>>8;

                    if( RadioText[A_B_Flag][Offset+3]!=0x00){
                        if(RadioText[A_B_Flag][Offset+3] != ( Group[3]&0x00FF ) ){
                            bzero(RadioText[A_B_Flag], sizeof(RadioText[0]));
                            NewRadioText[A_B_Flag] = true;
                        }
                    }
                    RadioText[A_B_Flag][Offset+3]=(Group[3]&0x00FF);

                }


            }
             ActivePage=A_B_Flag;


        } break;

        default:{

        } break;

        }
    }

bool rds_decoder::ReadRdsTitle( QString* QStrPtr ){
    bool Result=NewRadioText[ActivePage];
    /* We need to map the elements */
     *QStrPtr = QString("");
    for(uint32_t i=0;i<64;i++){
        if(RadioText[ActivePage][i]==0){
            break;
        } else {
            QStrPtr->append(RDSCharString(RadioText[ActivePage][i],CodeTable::G0));
        }
    }
    NewRadioText[ActivePage]=false;
    return Result;

}

bool rds_decoder::ReadRdsStation ( QString* QStrPtr ){

    bool Result=NewStationText;
    *QStrPtr = QString(StationText);
    NewStationText=false;
    return Result;

}

bool rds_decoder::RDS_Found( void ){
    return rdsfound;
}

/* taken form https://github.com/windytan/redsea/blob/master/src/tables.cc */
// EN 50067:1998, Annex E (pp. 73-76)
QString rds_decoder::RDSCharString(uint8_t code, CodeTable codetable) {
 QString result(" ");
  static const QString codetable_G0[] = {
      " ", "0", "@", "P", "‖", "p", "á", "â", "ª", "º", "Á", "Â", "Ã", "ã",
      "!", "1", "A", "Q", "a", "q", "à", "ä", "α", "¹", "À", "Ä", "Å", "å",
      "\"","2", "B", "R", "b", "r", "é", "ê", "©", "²", "É", "Ê", "Æ", "æ",
      "#", "3", "C", "S", "c", "s", "è", "ë", "‰", "³", "È", "Ë", "Œ", "œ",
      "¤", "4", "D", "T", "d", "t", "í", "î", "Ǧ", "±", "Í", "Î", "ŷ", "ŵ",
      "%", "5", "E", "U", "e", "u", "ì", "ï", "ě", "İ", "Ì", "Ï", "Ý", "ý",
      "&", "6", "F", "V", "f", "v", "ó", "ô", "ň", "ń", "Ó", "Ô", "Õ", "õ",
      "'", "7", "G", "W", "g", "w", "ò", "ö", "ő", "ű", "Ò", "Ö", "Ø", "ø",
      "(", "8", "H", "X", "h", "x", "ú", "û", "π", "µ", "Ú", "Û", "Þ", "þ",
      ")", "9", "I", "Y", "i", "y", "ù", "ü", "€", "¿", "Ù", "Ü", "Ŋ", "ŋ",
      "*", ":", "J", "Z", "j", "z", "Ñ", "ñ", "£", "÷", "Ř", "ř", "Ŕ", "ŕ",
      "+", ";", "K", "[", "k", "{", "Ç", "ç", "$", "°", "Č", "č", "Ć", "ć",
      ",", "<", "L", "\\","l", "|", "Ş", "ş", "←", "¼", "Š", "š", "Ś", "ś",
      "-", "=", "M", "]", "m", "}", "β", "ǧ", "↑", "½", "Ž", "ž", "Ź", "ź",
      ".", ">", "N", "―", "n", "¯", "¡", "ı", "→", "¾", "Ð", "đ", "Ŧ", "ŧ",
      "/", "?", "O", "_", "o", " ", "Ĳ", "ĳ", "↓", "§", "Ŀ", "ŀ", "ð" };

  static const QString codetable_G1[] ={
      " ", "0", "@", "P", "‖", "p", "á", "â", "ª", "º", "Є", "ý", "Π", "π",
      "!", "1", "A", "Q", "a", "q", "à", "ä", "ľ", "¹", "Я", "Љ", "α", "Ω",
      "\"","2", "B", "R", "b", "r", "é", "ê", "©", "²", "Б", "ď", "β", "ρ",
      "#", "3", "C", "S", "c", "s", "è", "ë", "‰", "³", "Ч", "Ш", "ψ", "σ",
      "¤", "4", "D", "T", "d", "t", "í", "î", "ǎ", "±", "Д", "Ц", "δ", "τ",
      "%", "5", "E", "U", "e", "u", "ì", "ï", "ě", "İ", "Э", "Ю", "ε", "ξ",
      "&", "6", "F", "V", "f", "v", "ó", "ô", "ň", "ń", "Ф", "Щ", "φ", "Θ",
      "'", "7", "G", "W", "g", "w", "ò", "ö", "ő", "ű", "Ѓ", "Њ", "γ", "Γ",
      "(", "8", "H", "X", "h", "x", "ú", "û", "ť", "ţ", "Ъ", "Џ", "η", "Ξ",
      ")", "9", "I", "Y", "i", "y", "ù", "ü", "€", "¿", "И", "Й", "ι", "υ",
      "*", ":", "J", "Z", "j", "z", "Ñ", "ñ", "£", "÷", "Ж", "З", "Σ", "ζ",
      "+", ";", "K", "[", "k", "{", "Ç", "ç", "$", "°", "Ќ", "č", "χ", "ς",
      ",", "<", "L", "\\","l", "|", "Ş", "ş", "←", "¼", "Л", "š", "λ", "Λ",
      "-", "=", "M", "]", "m", "}", "β", "ǧ", "↑", "½", "Ћ", "ž", "μ", "Ψ",
      ".", ">", "N", "―", "n", "¯", "¡", "ı", "→", "¾", "Ђ", "đ", "ν", "Δ",
      "/", "?", "O", "_", "o", " ", "Ĳ", "ĳ", "↓", "§", "Ы", "ć", "ω" };

  static const QString codetable_G2[] = {
      " ", "0", "@", "P", "‖", "p", "ب", "ظ", "א", "ן", "Є", "ý", "Π", "π",
      "!", "1", "A", "Q", "a", "q", "ت", "ع", "ב", "ס", "Я", "Љ", "α", "Ω",
      "\"","2", "B", "R", "b", "r", "ة", "غ", "ג", "ע", "Б", "ď", "β", "ρ",
      "#", "3", "C", "S", "c", "s", "ث", "ف", "ד", "פ", "Ч", "Ш", "ψ", "σ",
      "¤", "4", "D", "T", "d", "t", "ج", "ق", "ה", "ף", "Д", "Ц", "δ", "τ",
      "%", "5", "E", "U", "e", "u", "ح", "ك", "ו", "צ", "Э", "Ю", "ε", "ξ",
      "&", "6", "F", "V", "f", "v", "خ", "ل", "ז", "ץ", "Ф", "Щ", "φ", "Θ",
      "'", "7", "G", "W", "g", "w", "د", "م", "ח", "ץ", "Ѓ", "Њ", "γ", "Γ",
      "(", "8", "H", "X", "h", "x", "ذ", "ن", "ט", "ר", "Ъ", "Џ", "η", "Ξ",
      ")", "9", "I", "Y", "i", "y", "ر", "ه", "י", "ש", "И", "Й", "ι", "υ",
      "*", ":", "J", "Z", "j", "z", "ز", "و", "כ", "ת", "Ж", "З", "Σ", "ζ",
      "+", ";", "K", "[", "k", "{", "س", "ي", "ך", "°", "Ќ", "č", "χ", "ς",
      ",", "<", "L", "\\","l", "|", "ش", "←", "ל", "¼", "Л", "š", "λ", "Λ",
      "-", "=", "M", "]", "m", "}", "ص", "↑", "מ", "½", "Ћ", "ž", "μ", "Ψ",
      ".", ">", "N", "―", "n", "¯", "ض", "→", "ם", "¾", "Ђ", "đ", "ν", "Δ",
      "/", "?", "O", "_", "o", " ", "ط", "↓", "נ", "§", "Ы", "ć", "ω" };

  int row = code & 0xF;
  int col = code >> 4;
  int idx = row * 14 + (col - 2);
  if ( (col >= 2) && ( idx >= 0 ) && ( idx < ( sizeof(codetable_G0 ) / sizeof(codetable_G0[0]))) ) {
    switch (codetable) {
      case CodeTable::G0 :
        result = codetable_G0[idx];
        break;
      case CodeTable::G1 :
        result = codetable_G1[idx];
        break;
      case CodeTable::G2 :
        result = codetable_G2[idx];
        break;
    }
  }

  return result;
}


