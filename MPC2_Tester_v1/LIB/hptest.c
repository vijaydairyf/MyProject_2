// =============================================================================
//                                                             
// (c) Copyright NEDAP N.V. GROENLO HOLLAND             
//     ALL RIGHTS RESERVED                                     
//                                                             
// Description: Test Programma Modules for MPC                 
//                                                             
// =============================================================================

// include files  --------------------------------------------------------------
#include "io6811.h"
#include "intr6811.h"
#include "reg_flags.h"
#include "watchdog.h"
#include "iic.h"
#include "time.h"
#include "int.h"
#include "bnksel.h"                
#include "eeprom.h"                
#include "ramtest.h"     
#include "../lib/extport.h"
#include "../key/keyint.h"
#include "../include/cstartup.h"
#include "eehc11.h"
#include "hptest.h"
 
#pragma codeseg(SPECIALMODE)


#define FALSE   0
#define TRUE    1  
#define NR_OF_COMM_TESTS  10 
#define TOL     20
                                                                                                            
                                                                                                             
extern unsigned char BankRegCopy; 

//extern non_banked void ChecksumCurrentBank(void); 
//extern non_banked void RRomBankSel(unsigned char RomBank);


#if ( _HP_TEST_ )

// RomBank selectie ------------------------------------------------------------
non_banked void RRomBankSel(unsigned char RomBank)
{
  unsigned char value;

  value = BankRegCopy;
  value &= 0xe0;
  value |= RomBank;
  BankRegCopy = value;
  *(unsigned char *)BANK = value;
}


/***************************************************************/
/*Bepaal Checksum appl.                                        */
/***************************************************************/

non_banked unsigned char FTChecksumRom(unsigned char ProgChange) 
{
  unsigned long CheckSum;
  unsigned char i, *j, EEPromBanks, BankOld, Int;
  unsigned short EEPromStartAddress, EEPromEndAddress;

  EEPromStartAddress = *(unsigned short *)(Start_EE_Bank_Appl);
  EEPromEndAddress = *(unsigned short *)EE_Bank_Size_Appl + EEPromStartAddress;
  EEPromBanks = *(unsigned char *)Nr_EE_Banks_Appl;

  CheckSum = 0; 

  Interrupts_Off(&Int);

//  BankOld = BankACopy;

  //Bepaal checksum van alle banken waarin het applicatie programma zit
  for ( i = *(unsigned char *)(Start_PROG_Bank_Appl); i < EEPromBanks; i++) {
    Interrupts_On(&Int);
    WatchDog(); 
    Interrupts_Off(&Int);
    RRomBankSel(i);
    for (j = (unsigned char *)EEPromStartAddress; j < (unsigned char *)EEPromEndAddress; j++) {
      if ( ((unsigned short)j % 0x1000) == 0) {
        WatchDog();
//        ChecksumCurrentBank();
      }  
      CheckSum += *j;
    }
  }

  i = *(unsigned char *)BANK;
  i &= 0x1f;
  i |= BankOld;
  *(unsigned char *)BANK = i;
 
  if (CheckSum == (*(unsigned long *)(CheckSum_Appl)) ){ 
    Interrupts_On(&Int);
 
    //Mag waarde in EEProm al gewijzigd worden
    if(ProgChange == 1) {
      return(0);
    } else {
      return(0);
    }
  }
  Interrupts_On(&Int);
 
  //ProgPresent(FALSE);
  return(1);

}


// =============================================================================
non_banked unsigned short GetHpTestCommand(void)
{
  // wacht op byte, eerste byte moet F1 zijn ----------------------------------
  unsigned short r_val;
  unsigned char  x;

rcv_start:
  r_val = 0;
  while ( !(SCSR & 0x20 )) WatchDog();
  if ( SCSR & 0x0e ){
    // receive error -----------------------------------------------------------
    SCSR; SCDR; goto rcv_start; 
  }
  x = SCDR;
  if (( x == 0x1F ) || ( x == 0x21 ) ){
    r_val = ( SCDR << 8);
    while ( !(SCSR & 0x20 )) WatchDog();
    if ( (x=SCSR) & 0x0e ){
      // receive error -----------------------------------------------------------
       SCSR; SCDR; goto rcv_start; 
    }
    x = SCDR;
    return (r_val | x);
  } else  goto rcv_start;  
}


// =============================================================================
// Print "TESTPROG'                                   
// =============================================================================
non_banked void PrintTestProg(void) 
{
  unsigned short PointInfo;
  unsigned char  Text[11];

  Text[0] = 'T';
  Text[1] = 'E';
  Text[2] = 'S';
  Text[3] = 'T';
  Text[4] = 'P';
  Text[5] = 'R';
  Text[6] = 'O';
  Text[7] = 'G';
  Text[8] = ' ';
  Text[9] = ' ';
//  Text[8] = 0x30 + (EE_U1 / 10);
//  Text[9] = 0x30 + (EE_U1 % 10);
  Text[10] = 0;

  PointInfo = 0x0000;
  HandleWriteIic(KB_ADDRESS, IIC_WRITE_POINT_INFO, 2, (unsigned char *)&PointInfo ); WatchDog();
  HandleWriteIic(KB_ADDRESS, IIC_DISPLAY_TEXT, 10, Text); WatchDog();
}




// =============================================================================
non_banked unsigned char UartTest( void )
{              
  unsigned char  r_val;
  unsigned char  i;
   
  r_val = 0;  
  
  for ( i = 0; i < NR_OF_COMM_TESTS && r_val == 0 ; i++ ){ 
    // nog doen ???? -----------------------------------------------------------
    WatchDog();
  }
  return r_val;
}

// =============================================================================
non_banked void Send( unsigned short result)
{
  unsigned short time;
  unsigned char  x;

  while( !(x = ( SCSR & 0x80 )) );
  // disable receiver ----------------------------------------------------------
  SCCR2 &= ( 0xff- 0x24 );
  SCDR = (unsigned char)(result >> 8);
  while( !(x = ( SCSR & 0x80 )) );

  time = TCNT;
  while(!TimeControl(TCNT, time + 20000))WatchDog();

  SCDR = (unsigned char)(result & 0x00ff);
  while( !(x = ( SCSR & 0x80 )) );
  SCCR2 |= 0x04;

  time = TCNT;
  while(!TimeControl(TCNT, time + 20000))WatchDog();

}


// =============================================================================
non_banked void HpTest (void) 
{
  unsigned short HpTestCommand;
  unsigned short Result, time; 
  unsigned char cv[6];
  unsigned char stat;
 
  WatchDog();
  InitIic();
  enable_interrupt();
  PrintTestProg(); 

  RequestProgVersionsPIC();

  BAUD =  0x30;                 // Baudrate is 9600 
  SCCR1 = 0x00;                 // One start bit, eight databits, one stop bit,  
  SCCR2 |= 0x08;                                        // transmit enable

  WatchDog();              
  
  WriteEEpromByte( 0, &EEExtConMeasuring);
//  if (IC08PrgVrs == 0)IC08PrgVrs = 4;
  
  time = TCNT;
  while ( !TimeControl( TCNT, time +20000))WatchDog();
  Send(0xFF50);

  while(1) {
    WatchDog();              
    
    PrintTestProg();              
    HpTestCommand = 0;                             

    HpTestCommand = GetHpTestCommand(); 
  
    switch( HpTestCommand ) {
      case HP_ROM_TEST:
//           Result = FTChecksumRom(0);
           Result = 0;
           Send ( Result | 0xff50 );
        break;
      case HP_EEPROM_TEST:
           Send ( 0xff50 );                     // altijd ok.
        break;
      case HP_RAM_TEST:
           Result = RamTestAsm(0); 
           Send ( Result | 0xff50 );
        break;
      case HP_COND__LV_TEST:
      case HP_COND__LA_TEST:
      case HP_COND__RA_TEST:
      case HP_COND__RV_TEST:
           stat = HandleReadIic( PICA_ADDRESS, IIC_READ_COND, 5, (unsigned char *)cv );   
           if ( stat ) Send( 0xFF51 );
           else {
             if (cv[0] == IIC_READ_COND ) Send( (unsigned short)cv[ HpTestCommand - HP_COND__LV_TEST + 1] );
             else Send( 0xFF52 );
           }
        break;
      case HP_TEMP_TEST:
           stat = HandleReadIic( PICA_ADDRESS, IIC_READ_TEMP, 2, (unsigned char *)cv );   
           if ( stat ) Send( 0xFF51 );
           else {
             if (cv[0] == IIC_READ_TEMP ) Send( (unsigned short)cv[1] );
             else Send( 0xFF52 );
           } 
        break;     
      case HP_TEMP_REF1:   
           stat = HandleReadIic( PICA_ADDRESS, IIC_READ_TEMP, 2, (unsigned char *)cv );   
           if ( stat ) Send( 0xFF51 );
           else {
             if (cv[0] == IIC_READ_TEMP ){
               if ( (cv[1] >= 83) && (cv[1] <= 95 )) {
                 // ref '35' ok, store -----------------------------------------
                 WriteEEpromByte( cv[1], &EE_U1 ); 
                 if ( EE_U1 == cv[1] ) Send( 0xff53 ); 
                 else                  Send( 0xff52 );
               } else                  Send( 0xFF52 ); 
             } else                    Send( 0xFF52 );
           } 
        break;
      case HP_TEMP_REF2: 
           stat = HandleReadIic( PICA_ADDRESS, IIC_READ_TEMP, 2, (unsigned char *)cv );   
           if ( stat ) Send( 0xFF51 );
           else {
             if (cv[0] == IIC_READ_TEMP ){
               if ( (cv[1] >= 148) && (cv[1] <= 164 )) {
                 // ref '40' ok, store -----------------------------------------
                 WriteEEpromByte( cv[1], &EE_U2 ); 
                 if ( EE_U2 == cv[1] ) Send( 0xff54 ); 
                 else                  Send( 0xff52 );
               } else                  Send( 0xFF52 ); 
             } else                    Send( 0xFF52 );
           } 
        break;
      case HP_RTN_REF1: 
           Send( EE_U1 ); 
        break;
      case HP_RTN_REF2: 
           Send( EE_U2 ); 
         break;
      case HP_WD_TEST: 
           if ( CONFIG & NOCOP ) Send( 0xFF51 );
           else                  Send( 0xFF50 ); 
        break;
      default:
           if ( HpTestCommand > 0x2100 && HpTestCommand < 0x212f ){
             if ( HandleWriteIic(PICA_ADDRESS, IIC_WRITE_PORTB,   1, (unsigned char *)((unsigned char *)(&HpTestCommand)+1)) ){
               Send ( 0xff51 );
             } else {
               Send ( 0xff50 );
             }
           }
        break;   
    }
  }
}

  
#endif 