/*
 * InfoNES mapper 252 compatibility body.
 * Source body adapted from pico-infonesPlus/infones/mapper.
 */

#include "nes_mapper_infones.h"

#if INFONES_ENABLE_MAPPER_252

/* ---- InfoNES_Mapper_252.cpp ---- */
/*===================================================================*/
/*                                                                   */
/*              Mapper 252 : Sangokushi - Chuugen no hasha           */
/*                                                                   */
/*===================================================================*/

BYTE	*Map252_Chr_Ram;
BYTE	Map252_Reg[9];
BYTE	Map252_IRQ_Enable;
BYTE	Map252_IRQ_Counter;
BYTE	Map252_IRQ_Latch;
BYTE	Map252_IRQ_Occur;
int	Map252_IRQ_Clock;

/* 外星科技 Mapper 252 规范：1KB CHR 页 6、7 映射到板载 2KB CHR-RAM（用于动态写入汉字字模），其余映射 CHR-ROM */
static void Map252_Set_Chr_Bank(int slot)
{
  BYTE bank = Map252_Reg[slot];
  if ((bank == 6 || bank == 7) && Map252_Chr_Ram) {
    PPUBANK[slot] = Map252_Chr_Ram + ((DWORD)(bank - 6) * 0x400);
  } else if (NesHeader.byVRomSize > 0) {
    PPUBANK[slot] = VROMPAGE(bank % (NesHeader.byVRomSize << 3));
  }
  InfoNES_SetupChr();
}

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 252                                            */
/*-------------------------------------------------------------------*/
void Map252_Init()
{
  /* Initialize Mapper */
  MapperInit = Map252_Init;

  /* Write to Mapper */
  MapperWrite = Map252_Write;

  /* Write to SRAM */
  MapperSram = Map0_Sram;

  /* Write to APU */
  MapperApu = Map0_Apu;

  /* Read from APU */
  MapperReadApu = Map0_ReadApu;

  /* Callback at VSync */
  MapperVSync = Map0_VSync;

  /* Callback at HSync */
  MapperHSync = Map252_HSync;

  /* Callback at PPU */
  MapperPPU = Map0_PPU;

  /* Callback at Rendering Screen ( 1:BG, 0:Sprite ) */
  MapperRenderScreen = Map0_RenderScreen;

  /* Set SRAM Banks */
  SRAMBANK = SRAM;

  /* 分配 2KB 独立 CHR-RAM（bank 6、7）并清零 */
  if (!Map252_Chr_Ram) {
    Map252_Chr_Ram = (BYTE *)nes_malloc(0x800);
  }
  if (Map252_Chr_Ram) {
    InfoNES_MemorySet(Map252_Chr_Ram, 0, 0x800);
  }

  /* Set Registers */
  for( int i = 0; i < 9; i++ ) {
    Map252_Reg[i] = i;
  }

  Map252_IRQ_Enable = 0;
  Map252_IRQ_Counter = 0;
  Map252_IRQ_Latch = 0;
  Map252_IRQ_Clock = 0;
  Map252_IRQ_Occur = 0;

  /* Set ROM Banks */
  ROMBANK0 = ROMPAGE( 0 );
  ROMBANK1 = ROMPAGE( 1 );
  ROMBANK2 = ROMLASTPAGE( 1 );
  ROMBANK3 = ROMLASTPAGE( 0 );

  /* Set PPU Banks */
  if ( NesHeader.byVRomSize > 0 ) {
    for ( int nPage = 0; nPage < 8; ++nPage )
      Map252_Set_Chr_Bank( nPage );
  }

  /* Set up wiring of the interrupt pin */
  K6502_Set_Int_Wiring( 1, 1 );
}

/*-------------------------------------------------------------------*/
/*  Mapper 252 Write Function                                        */
/*-------------------------------------------------------------------*/
void Map252_Write( WORD wAddr, BYTE byData )
{
  if( (wAddr & 0xF000) == 0x8000 ) {
    ROMBANK0 = ROMPAGE( byData % (NesHeader.byRomSize<<1));
    return;
  }
  if( (wAddr & 0xF000) == 0xA000 ) {
    ROMBANK1 = ROMPAGE( byData % (NesHeader.byRomSize<<1));
    return;
  }
  switch( wAddr & 0xF00C ) {
  case 0x9000:
  case 0x9004:
    /* VRC4e 镜像模式控制寄存器 */
    switch( byData & 0x03 ) {
    case 0:
      InfoNES_Mirroring( 1 );
      break;
    case 1:
      InfoNES_Mirroring( 0 );
      break;
    case 2:
      InfoNES_Mirroring( 3 );
      break;
    case 3:
      InfoNES_Mirroring( 2 );
      break;
    }
    break;

  case 0xB000:
    Map252_Reg[0] = (Map252_Reg[0] & 0xF0) | (byData & 0x0F);
    Map252_Set_Chr_Bank( 0 );
    break;
  case 0xB004:
    Map252_Reg[0] = (Map252_Reg[0] & 0x0F) | ((byData & 0x0F) << 4);
    Map252_Set_Chr_Bank( 0 );
    break;
  case 0xB008:
    Map252_Reg[1] = (Map252_Reg[1] & 0xF0) | (byData & 0x0F);
    Map252_Set_Chr_Bank( 1 );
    break;
  case 0xB00C:
    Map252_Reg[1] = (Map252_Reg[1] & 0x0F) | ((byData & 0x0F) << 4);
    Map252_Set_Chr_Bank( 1 );
    break;

  case 0xC000:
    Map252_Reg[2] = (Map252_Reg[2] & 0xF0) | (byData & 0x0F);
    Map252_Set_Chr_Bank( 2 );
    break;
  case 0xC004:
    Map252_Reg[2] = (Map252_Reg[2] & 0x0F) | ((byData & 0x0F) << 4);
    Map252_Set_Chr_Bank( 2 );
    break;
  case 0xC008:
    Map252_Reg[3] = (Map252_Reg[3] & 0xF0) | (byData & 0x0F);
    Map252_Set_Chr_Bank( 3 );
    break;
  case 0xC00C:
    Map252_Reg[3] = (Map252_Reg[3] & 0x0F) | ((byData & 0x0F) << 4);
    Map252_Set_Chr_Bank( 3 );
    break;

  case 0xD000:
    Map252_Reg[4] = (Map252_Reg[4] & 0xF0) | (byData & 0x0F);
    Map252_Set_Chr_Bank( 4 );
    break;
  case 0xD004:
    Map252_Reg[4] = (Map252_Reg[4] & 0x0F) | ((byData & 0x0F) << 4);
    Map252_Set_Chr_Bank( 4 );
    break;
  case 0xD008:
    Map252_Reg[5] = (Map252_Reg[5] & 0xF0) | (byData & 0x0F);
    Map252_Set_Chr_Bank( 5 );
    break;
  case 0xD00C:
    Map252_Reg[5] = (Map252_Reg[5] & 0x0F) | ((byData & 0x0F) << 4);
    Map252_Set_Chr_Bank( 5 );
    break;

  case 0xE000:
    Map252_Reg[6] = (Map252_Reg[6] & 0xF0) | (byData & 0x0F);
    Map252_Set_Chr_Bank( 6 );
    break;
  case 0xE004:
    Map252_Reg[6] = (Map252_Reg[6] & 0x0F) | ((byData & 0x0F) << 4);
    Map252_Set_Chr_Bank( 6 );
    break;
  case 0xE008:
    Map252_Reg[7] = (Map252_Reg[7] & 0xF0) | (byData & 0x0F);
    Map252_Set_Chr_Bank( 7 );
    break;
  case 0xE00C:
    Map252_Reg[7] = (Map252_Reg[7] & 0x0F) | ((byData & 0x0F) << 4);
    Map252_Set_Chr_Bank( 7 );
    break;

  case 0xF000:
    Map252_IRQ_Latch = (Map252_IRQ_Latch & 0xF0) | (byData & 0x0F);
    Map252_IRQ_Occur = 0;
    break;
  case 0xF004:
    Map252_IRQ_Latch = (Map252_IRQ_Latch & 0x0F) | ((byData & 0x0F) << 4);
    Map252_IRQ_Occur = 0;
    break;

  case 0xF008:
    Map252_IRQ_Enable = byData & 0x03;
    if( Map252_IRQ_Enable & 0x02 ) {
      Map252_IRQ_Counter = Map252_IRQ_Latch;
      Map252_IRQ_Clock = 0;
    }
    Map252_IRQ_Occur = 0;
    break;

  case 0xF00C:
    Map252_IRQ_Enable = (Map252_IRQ_Enable & 0x01) * 3;
    Map252_IRQ_Occur = 0;
    break;
  }
}

/*-------------------------------------------------------------------*/
/*  Mapper 252 H-Sync Function                                       */
/*-------------------------------------------------------------------*/
void Map252_HSync()
{
  if( Map252_IRQ_Enable & 0x02 ) {
    if( Map252_IRQ_Counter == 0xFF ) {
      Map252_IRQ_Occur = 0xFF;
      Map252_IRQ_Counter = Map252_IRQ_Latch;
      Map252_IRQ_Enable = (Map252_IRQ_Enable & 0x01) * 3;
    } else {
      Map252_IRQ_Counter++;
    }
  }
  if( Map252_IRQ_Occur ) {
    IRQ_REQ;
  }
}




#endif /* INFONES_ENABLE_MAPPER_252 */
