/****************************************************************************************
|  Description: bootloader flash driver source file
|    File Name: flash.c
|
|----------------------------------------------------------------------------------------
|                          C O P Y R I G H T
|----------------------------------------------------------------------------------------
|   Copyright (c) 2012  by Feaser    http://www.feaser.com    All rights reserved
|
|----------------------------------------------------------------------------------------
|                            L I C E N S E
|----------------------------------------------------------------------------------------
| This file is part of OpenBLT. OpenBLT is free software: you can redistribute it and/or
| modify it under the terms of the GNU General Public License as published by the Free
| Software Foundation, either version 3 of the License, or (at your option) any later
| version.
|
| OpenBLT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
| without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
| PURPOSE. See the GNU General Public License for more details.
|
| You should have received a copy of the GNU General Public License along with OpenBLT.
| If not, see <http://www.gnu.org/licenses/>.
|
| A special exception to the GPL is included to allow you to distribute a combined work 
| that includes OpenBLT without being obliged to provide the source code for any 
| proprietary components. The exception text is included at the bottom of the license
| file <license.html>.
| 
****************************************************************************************/

/****************************************************************************************
* Include files
****************************************************************************************/
#include "boot.h"                                /* bootloader generic header          */
#include "efm32_msc.h"                           /* MSC driver from EFM32 library      */


/****************************************************************************************
* Macro definitions
****************************************************************************************/
#define FLASH_INVALID_SECTOR            (0xff)
#define FLASH_INVALID_ADDRESS           (0xffffffff)
#define FLASH_WRITE_BLOCK_SIZE          (512)
#define FLASH_TOTAL_SECTORS             (sizeof(flashLayout)/sizeof(flashLayout[0]))
#define FLASH_VECTOR_TABLE_CS_OFFSET    (0x0B8)


/****************************************************************************************
* Type definitions
****************************************************************************************/
/* flash sector descriptor type */
typedef struct 
{
  blt_addr   sector_start;                       /* sector start address               */
  blt_int32u sector_size;                        /* sector size in bytes               */
  blt_int8u  sector_num;                         /* sector number                      */
} tFlashSector;                                  /* flash sector description           */

/* programming is done per block of max FLASH_WRITE_BLOCK_SIZE. for this a flash block
 * manager is implemented in this driver. this flash block manager depends on this
 * flash block info structure. It holds the base address of the flash block and the
 * data that should be programmed into the flash block. The .base_addr must be a multiple 
 * of FLASH_WRITE_BLOCK_SIZE.
 */
typedef struct
{
  blt_addr  base_addr;
  blt_int8u data[FLASH_WRITE_BLOCK_SIZE];
} tFlashBlockInfo;


/****************************************************************************************
* Function prototypes
****************************************************************************************/
static blt_bool   FlashInitBlock(tFlashBlockInfo *block, blt_addr address);
static tFlashBlockInfo *FlashSwitchBlock(tFlashBlockInfo *block, blt_addr base_addr);
static blt_bool   FlashAddToBlock(tFlashBlockInfo *block, blt_addr address, 
                                  blt_int8u *data, blt_int16u len);
static blt_bool   FlashWriteBlock(tFlashBlockInfo *block);
static blt_bool   FlashEraseSectors(blt_int8u first_sector, blt_int8u last_sector);
static blt_int8u  FlashGetSector(blt_addr address);
static blt_addr   FlashGetSectorBaseAddr(blt_int8u sector);
static blt_addr   FlashGetSectorSize(blt_int8u sector);
static blt_int32u FlashCalcPageSize(void);


/****************************************************************************************
* Local constant declarations
****************************************************************************************/
/* The current flash layout does not reflect the minimum sector size of the physical
 * flash (1 - 2kb), because this would make the table quit long and a waste of ROM. The
 * minimum sector size is only really needed when erasing the flash. This can still be
 * done in combination with macro FLASH_ERASE_BLOCK_SIZE.
 */
static const tFlashSector flashLayout[] =
{
  /* { 0x00000000, 0x02000,  0},           flash sector  0 - reserved for bootloader   */
  { 0x00002000, 0x02000,  1},           /* flash sector  1 - 8kb                       */
  { 0x00004000, 0x02000,  2},           /* flash sector  2 - 8kb                       */
  { 0x00006000, 0x02000,  3},           /* flash sector  3 - 8kb                       */
#if (BOOT_NVM_SIZE_KB > 32)
  { 0x00008000, 0x02000,  4},           /* flash sector  4 - 8kb                       */
  { 0x0000A000, 0x02000,  5},           /* flash sector  5 - 8kb                       */
  { 0x0000C000, 0x02000,  6},           /* flash sector  6 - 8kb                       */
  { 0x0000E000, 0x02000,  7},           /* flash sector  7 - 8kb                       */
#endif
#if (BOOT_NVM_SIZE_KB > 64)
  { 0x00010000, 0x02000,  8},           /* flash sector  8 - 8kb                       */
  { 0x00012000, 0x02000,  9},           /* flash sector  9 - 8kb                       */
  { 0x00014000, 0x02000, 10},           /* flash sector 10 - 8kb                       */
  { 0x00016000, 0x02000, 11},           /* flash sector 11 - 8kb                       */
  { 0x00018000, 0x02000, 12},           /* flash sector 12 - 8kb                       */
  { 0x0001A000, 0x02000, 13},           /* flash sector 13 - 8kb                       */
  { 0x0001C000, 0x02000, 14},           /* flash sector 14 - 8kb                       */
  { 0x0001E000, 0x02000, 15},           /* flash sector 15 - 8kb                       */
#endif
#if (BOOT_NVM_SIZE_KB > 128)
  { 0x00020000, 0x08000, 16},           /* flash sector 16 - 32kb                      */
  { 0x00028000, 0x08000, 17},           /* flash sector 17 - 32kb                      */
  { 0x00030000, 0x08000, 18},           /* flash sector 18 - 32kb                      */
  { 0x00038000, 0x08000, 19},           /* flash sector 19 - 32kb                      */
#endif
#if (BOOT_NVM_SIZE_KB > 256)
  { 0x00040000, 0x08000, 20},           /* flash sector 20 - 32kb                      */
  { 0x00048000, 0x08000, 21},           /* flash sector 21 - 32kb                      */
  { 0x00050000, 0x08000, 22},           /* flash sector 22 - 32kb                      */
  { 0x00058000, 0x08000, 23},           /* flash sector 23 - 32kb                      */
  { 0x00060000, 0x08000, 24},           /* flash sector 24 - 32kb                      */
  { 0x00068000, 0x08000, 25},           /* flash sector 25 - 32kb                      */
  { 0x00070000, 0x08000, 26},           /* flash sector 26 - 32kb                      */
  { 0x00078000, 0x08000, 27},           /* flash sector 27 - 32kb                      */
#endif
#if (BOOT_NVM_SIZE_KB > 512)
#error "BOOT_NVM_SIZE_KB > 512 is currently not supported."
#endif
};


/****************************************************************************************
* Local data declarations
****************************************************************************************/
/* The smallest amount of flash that can be programmed is FLASH_WRITE_BLOCK_SIZE. A flash
 * block manager is implemented in this driver and stores info in this variable. Whenever
 * new data should be flashed, it is first added to a RAM buffer, which is part of this
 * variable. Whenever the RAM buffer, which has the size of a flash block, is full or 
 * data needs to be written to a different block, the contents of the RAM buffer are 
 * programmed to flash. The flash block manager requires some software overhead, yet
 * results is faster flash programming because data is first harvested, ideally until
 * there is enough to program an entire flash block, before the flash device is actually
 * operated on.
 */
static tFlashBlockInfo blockInfo;

/* The first block of the user program holds the vector table, which on the STM32 is
 * also the where the checksum is written to. Is it likely that the vector table is
 * first flashed and then, at the end of the programming sequence, the checksum. This
 * means that this flash block need to be written to twice. Normally this is not a 
 * problem with flash memory, as long as you write the same values to those bytes that
 * are not supposed to be changed and the locations where you do write to are still in
 * the erased 0xFF state. Unfortunately, writing twice to flash this way, does not work
 * reliably on all micros. This is why we need to have an extra block, the bootblock,
 * placed under the management of the block manager. This way is it possible to implement
 * functionality so that the bootblock is only written to once at the end of the 
 * programming sequency.
 */
static tFlashBlockInfo bootBlockInfo;


/****************************************************************************************
** NAME:           FlashInit
** PARAMETER:      none
** RETURN VALUE:   none
** DESCRIPTION:    Initializes the flash driver. 
**
****************************************************************************************/
void FlashInit(void)
{
  /* enable the flash controller for writing */
  MSC_Init();
  /* init the flash block info structs by setting the address to an invalid address */
  blockInfo.base_addr = FLASH_INVALID_ADDRESS;
  bootBlockInfo.base_addr = FLASH_INVALID_ADDRESS;
} /*** end of FlashInit ***/


/****************************************************************************************
** NAME:           FlashWrite
** PARAMETER:      addr start address
**                 len  length in bytes
**                 data pointer to the data buffer.
** RETURN VALUE:   BLT_TRUE if successful, BLT_FALSE otherwise. 
** DESCRIPTION:    Writes the data to flash through a flash block manager. Note that this
**                 function also checks that no data is programmed outside the flash 
**                 memory region, so the bootloader can never be overwritten.
**
****************************************************************************************/
blt_bool FlashWrite(blt_addr addr, blt_int32u len, blt_int8u *data)
{
  blt_addr base_addr;

  /* make sure the addresses are within the flash device */
  if ( (FlashGetSector(addr) == FLASH_INVALID_SECTOR) || \
       (FlashGetSector(addr+len-1) == FLASH_INVALID_SECTOR) )
  {
    return BLT_FALSE;       
  }

  /* if this is the bootblock, then let the boot block manager handle it */
  base_addr = (addr/FLASH_WRITE_BLOCK_SIZE)*FLASH_WRITE_BLOCK_SIZE;
  if (base_addr == flashLayout[0].sector_start)
  {
    /* let the boot block manager handle it */
    return FlashAddToBlock(&bootBlockInfo, addr, data, len);
  }
  /* let the block manager handle it */
  return FlashAddToBlock(&blockInfo, addr, data, len);
} /*** end of FlashWrite ***/


/****************************************************************************************
** NAME:           FlashErase
** PARAMETER:      addr start address
**                 len  length in bytes
** RETURN VALUE:   BLT_TRUE if successful, BLT_FALSE otherwise.
** DESCRIPTION:    Erases the flash memory. Note that this function also checks that no 
**                 data is erased outside the flash memory region, so the bootloader can 
**                 never be erased.
**
****************************************************************************************/
blt_bool FlashErase(blt_addr addr, blt_int32u len)
{
  blt_int8u first_sector;
  blt_int8u last_sector;
  
  /* obtain the first and last sector number */
  first_sector = FlashGetSector(addr);
  last_sector  = FlashGetSector(addr+len-1);
  /* check them */
  if ( (first_sector == FLASH_INVALID_SECTOR) || (last_sector == FLASH_INVALID_SECTOR) )
  {
    return BLT_FALSE;
  }
  /* erase the sectors */
  return FlashEraseSectors(first_sector, last_sector);
} /*** end of FlashErase ***/


/****************************************************************************************
** NAME:           FlashWriteChecksum
** PARAMETER:      none
** RETURN VALUE:   BLT_TRUE is successful, BTL_FALSE otherwise.
** DESCRIPTION:    Writes a checksum of the user program to non-volatile memory. This is
**                 performed once the entire user program has been programmed. Through
**                 the checksum, the bootloader can check if the programming session
**                 was completed, which indicates that a valid user programming is
**                 present and can be started.
**
****************************************************************************************/
blt_bool FlashWriteChecksum(void)
{
  blt_int32u signature_checksum = 0;
  
  /* for the STM32 target we defined the checksum as the Two's complement value of the
   * sum of the first 7 exception addresses.
   *
   * Layout of the vector table:
   *    0x00000000 Initial stack pointer 
   *    0x00000004 Reset Handler
   *    0x00000008 NMI Handler
   *    0x0000000C Hard Fault Handler
   *    0x00000010 MPU Fault Handler 
   *    0x00000014 Bus Fault Handler
   *    0x00000018 Usage Fault Handler
   *
   *    signature_checksum = Two's complement of (SUM(exception address values))
   *   
   *    the bootloader writes this 32-bit checksum value right after the vector table
   *    of the user program. note that this means one extra dummy entry must be added
   *    at the end of the user program's vector table to reserve storage space for the
   *    checksum.
   */

  /* first check that the bootblock contains valid data. if not, this means the
   * bootblock is not part of the reprogramming this time and therefore no
   * new checksum needs to be written
   */
   if (bootBlockInfo.base_addr == FLASH_INVALID_ADDRESS)
   {
    return BLT_TRUE;
   }

  /* compute the checksum. note that the user program's vectors are not yet written
   * to flash but are present in the bootblock data structure at this point.
   */
  signature_checksum += *((blt_int32u*)(&bootBlockInfo.data[0+0x00]));
  signature_checksum += *((blt_int32u*)(&bootBlockInfo.data[0+0x04]));
  signature_checksum += *((blt_int32u*)(&bootBlockInfo.data[0+0x08]));
  signature_checksum += *((blt_int32u*)(&bootBlockInfo.data[0+0x0C]));
  signature_checksum += *((blt_int32u*)(&bootBlockInfo.data[0+0x10]));
  signature_checksum += *((blt_int32u*)(&bootBlockInfo.data[0+0x14]));
  signature_checksum += *((blt_int32u*)(&bootBlockInfo.data[0+0x18]));
  signature_checksum  = ~signature_checksum; /* one's complement */
  signature_checksum += 1; /* two's complement */

  /* write the checksum */
  return FlashWrite(flashLayout[0].sector_start+FLASH_VECTOR_TABLE_CS_OFFSET, 
                    sizeof(blt_addr), (blt_int8u*)&signature_checksum);
} /*** end of FlashWriteChecksum ***/


/****************************************************************************************
** NAME:           FlashVerifyChecksum
** PARAMETER:      none
** RETURN VALUE:   BLT_TRUE is successful, BTL_FALSE otherwise.
** DESCRIPTION:    Verifies the checksum, which indicates that a valid user program is
**                 present and can be started.
**
****************************************************************************************/
blt_bool FlashVerifyChecksum(void)
{
  blt_int32u signature_checksum = 0;
  
  /* verify the checksum based on how it was written by CpuWriteChecksum() */
  signature_checksum += *((blt_int32u*)(flashLayout[0].sector_start));
  signature_checksum += *((blt_int32u*)(flashLayout[0].sector_start+0x04));
  signature_checksum += *((blt_int32u*)(flashLayout[0].sector_start+0x08));
  signature_checksum += *((blt_int32u*)(flashLayout[0].sector_start+0x0C));
  signature_checksum += *((blt_int32u*)(flashLayout[0].sector_start+0x10));
  signature_checksum += *((blt_int32u*)(flashLayout[0].sector_start+0x14));
  signature_checksum += *((blt_int32u*)(flashLayout[0].sector_start+0x18));
  signature_checksum += *((blt_int32u*)(flashLayout[0].sector_start+FLASH_VECTOR_TABLE_CS_OFFSET));
  /* sum should add up to an unsigned 32-bit value of 0 */
  if (signature_checksum == 0)
  {
    /* checksum okay */
    return BLT_TRUE;
  }
  /* checksum incorrect */
  return BLT_FALSE;
} /*** end of FlashVerifyChecksum ***/


/****************************************************************************************
** NAME:           FlashDone
** PARAMETER:      none
** RETURN VALUE:   BLT_TRUE is succesful, BLT_FALSE otherwise.
** DESCRIPTION:    Finilizes the flash driver operations. There could still be data in
**                 the currently active block that needs to be flashed.
**
****************************************************************************************/
blt_bool FlashDone(void)
{
  /* check if there is still data waiting to be programmed in the boot block */
  if (bootBlockInfo.base_addr != FLASH_INVALID_ADDRESS)
  {
    if (FlashWriteBlock(&bootBlockInfo) == BLT_FALSE)
    {
      return BLT_FALSE;
    }
  }
  
  /* check if there is still data waiting to be programmed */
  if (blockInfo.base_addr != FLASH_INVALID_ADDRESS)
  {
    if (FlashWriteBlock(&blockInfo) == BLT_FALSE)
    {
      return BLT_FALSE;
    }
  }
  /* disable the flash controller for writing */
  MSC_Deinit();
  /* still here so all is okay */  
  return BLT_TRUE;
} /*** end of FlashDone ***/


/****************************************************************************************
** NAME:           FlashInitBlock
** PARAMETER:      block   pointer to flash block info structure to operate on.
**                 address base address of the block data.
** RETURN VALUE:   BLT_TRUE is succesful, BLT_FALSE otherwise.
** DESCRIPTION:    Copies data currently in flash to the block->data and sets the 
**                 base address.
**
****************************************************************************************/
static blt_bool FlashInitBlock(tFlashBlockInfo *block, blt_addr address)
{
  /* check address alignment */  
  if ((address % FLASH_WRITE_BLOCK_SIZE) != 0)
  {
    return BLT_FALSE;
  }
  /* make sure that we are initializing a new block and not the same one */
  if (block->base_addr == address)
  {
    /* block already initialized, so nothing to do */
    return BLT_TRUE;
  }
  /* set the base address and copies the current data from flash */  
  block->base_addr = address;  
  CpuMemCopy((blt_addr)block->data, address, FLASH_WRITE_BLOCK_SIZE);
  return BLT_TRUE;
} /*** end of FlashInitBlock ***/


/****************************************************************************************
** NAME:           FlashSwitchBlock
** PARAMETER:      block     pointer to flash block info structure to operate on.
**                 base_addr base address for the next block
** RETURN VALUE:   the pointer of the block info struct that is no being used, or a NULL
**                 pointer in case of error.
** DESCRIPTION:    Switches blocks by programming the current one and initializing the
**                 next.
**
****************************************************************************************/
static tFlashBlockInfo *FlashSwitchBlock(tFlashBlockInfo *block, blt_addr base_addr)
{
  /* check if a switch needs to be made away from the boot block. in this case the boot
   * block shouldn't be written yet, because this is done at the end of the programming
   * session by FlashDone(), this is right after the checksum was written. 
   */
  if (block == &bootBlockInfo)
  {
    /* switch from the boot block to the generic block info structure */
    block = &blockInfo;
  }
  /* check if a switch back into the bootblock is needed. in this case the generic block 
   * doesn't need to be written here yet.
   */
  else if (base_addr == flashLayout[0].sector_start)
  {
    /* switch from the generic block to the boot block info structure */
    block = &bootBlockInfo;
    base_addr = flashLayout[0].sector_start;
  }
  else
  {
    /* need to switch to a new block, so program the current one and init the next */
    if (FlashWriteBlock(block) == BLT_FALSE)
    {
      return BLT_NULL;
    }
  }

  /* initialize tne new block when necessary */
  if (FlashInitBlock(block, base_addr) == BLT_FALSE) 
  {
    return BLT_NULL;
  }

  /* still here to all is okay  */
  return block;
} /*** end of FlashSwitchBlock ***/


/****************************************************************************************
** NAME:           FlashAddToBlock
** PARAMETER:      block   pointer to flash block info structure to operate on.
**                 address flash destination address
**                 data    pointer to the byte array with data
**                 len     number of bytes to add to the block
** RETURN VALUE:   BLT_TRUE if successful, BLT_FALSE otherwise.
** DESCRIPTION:    Programming is done per block. This function adds data to the block
**                 that is currently collecting data to be written to flash. If the
**                 address is outside of the current block, the current block is written
**                 to flash an a new block is initialized.
**
****************************************************************************************/
static blt_bool FlashAddToBlock(tFlashBlockInfo *block, blt_addr address, 
                                blt_int8u *data, blt_int16u len)
{
  blt_addr   current_base_addr;
  blt_int8u  *dst;
  blt_int8u  *src;
  
  /* determine the current base address */
  current_base_addr = (address/FLASH_WRITE_BLOCK_SIZE)*FLASH_WRITE_BLOCK_SIZE;

  /* make sure the blockInfo is not uninitialized */
  if (block->base_addr == FLASH_INVALID_ADDRESS)
  {
    /* initialize the blockInfo struct for the current block */
    if (FlashInitBlock(block, current_base_addr) == BLT_FALSE)
    {
      return BLT_FALSE;
    }
  }

  /* check if the new data fits in the current block */
  if (block->base_addr != current_base_addr)
  {
    /* need to switch to a new block, so program the current one and init the next */
    block = FlashSwitchBlock(block, current_base_addr);
    if (block == BLT_NULL)
    {
      return BLT_FALSE;
    }
  }
  
  /* add the data to the current block, but check for block overflow */
  dst = &(block->data[address - block->base_addr]);
  src = data;
  do
  {
    /* keep the watchdog happy */
    CopService();
    /* buffer overflow? */
    if ((blt_addr)(dst-&(block->data[0])) >= FLASH_WRITE_BLOCK_SIZE)
    {
      /* need to switch to a new block, so program the current one and init the next */
      block = FlashSwitchBlock(block, current_base_addr+FLASH_WRITE_BLOCK_SIZE);
      if (block == BLT_NULL)
      {
        return BLT_FALSE;
      }
      /* reset destination pointer */
      dst = &(block->data[0]);
    }
    /* write the data to the buffer */
    *dst = *src;
    /* update pointers */
    dst++;
    src++;
    /* decrement byte counter */
    len--;
  }
  while (len > 0);
  /* still here so all is good */
  return BLT_TRUE;
} /*** end of FlashAddToBlock ***/


/****************************************************************************************
** NAME:           FlashWriteBlock
** PARAMETER:      block pointer to flash block info structure to operate on.
** RETURN VALUE:   BLT_TRUE if successful, BLT_FALSE otherwise.
** DESCRIPTION:    Programs FLASH_WRITE_BLOCK_SIZE bytes to flash from the block->data
**                 array. 
**
****************************************************************************************/
static blt_bool FlashWriteBlock(tFlashBlockInfo *block)
{
  blt_int8u  sector_num;
  blt_bool   result = BLT_TRUE;
  blt_addr   prog_addr;
  blt_int32u prog_data;
  blt_int32u word_cnt;


  /* check that address is actually within flash */
  sector_num = FlashGetSector(block->base_addr);
  if (sector_num == FLASH_INVALID_SECTOR)
  {
    return BLT_FALSE;
  }

  /* program all words in the block one by one */
  for (word_cnt=0; word_cnt<(FLASH_WRITE_BLOCK_SIZE/sizeof(blt_int32u)); word_cnt++)
  {
    prog_addr = block->base_addr + (word_cnt * sizeof(blt_int32u));
    prog_data = *(volatile blt_int32u*)(&block->data[word_cnt * sizeof(blt_int32u)]);
    /* keep the watchdog happy */
    CopService();
    /* program a word */
    if (MSC_WriteWord((uint32_t *)prog_addr, &prog_data, sizeof(blt_int32u)) != mscReturnOk)
    {
      result = BLT_FALSE;
      break;
    }
    /* verify that the written data is actually there */
    if (*(volatile blt_int32u*)prog_addr != prog_data)
    {
      result = BLT_FALSE;
      break;
    }
  }
  /* still here so all is okay */
  return result;
} /*** end of FlashWriteBlock ***/


/****************************************************************************************
** NAME:           FlashCalcPageSize
** PARAMETER:      none
** RETURN VALUE:   The flash page size
** DESCRIPTION:    Determines the flash page size for the specific EFM32 derivative. This
**                 is the minimum erase size.
**
****************************************************************************************/
static blt_int32u FlashCalcPageSize(void)
{
  blt_int8u family = *(blt_int8u*)0x0FE081FE;

  if ( ( family == 71 ) || ( family == 73 ) )
  {
    /* Gecko and Tiny, 'G' or 'I' */
    return 512;                
  }
  else if ( family == 72 )
  {
    /* Giant, 'H' */
    return 4096;
  }
  else
  {
    /* Leopard, 'J' */
    return 2048;
  }
} /*** end of FlashCalcPageSize ***/


/****************************************************************************************
** NAME:           FlashEraseSectors
** PARAMETER:      first_sector first flash sector number
**                 last_sector  last flash sector number
** RETURN VALUE:   BLT_TRUE if successful, BLT_FALSE otherwise.
** DESCRIPTION:    Erases the flash sectors from first_sector up until last_sector
**
****************************************************************************************/
static blt_bool FlashEraseSectors(blt_int8u first_sector, blt_int8u last_sector)
{
  blt_int16u nr_of_blocks;
  blt_int16u block_cnt;
  blt_addr   start_addr;
  blt_addr   end_addr;
  blt_int32u erase_block_size;

  /* validate the sector numbers */
  if (first_sector > last_sector)
  {
    return BLT_FALSE;
  }
  if ( (first_sector < flashLayout[0].sector_num) || \
       (last_sector > flashLayout[FLASH_TOTAL_SECTORS-1].sector_num) )
  {
    return BLT_FALSE;
  }
  /* determine the minimum erase size */
  erase_block_size = FlashCalcPageSize();
  /* determine how many blocks need to be erased */
  start_addr = FlashGetSectorBaseAddr(first_sector);
  end_addr = FlashGetSectorBaseAddr(last_sector) + FlashGetSectorSize(last_sector) - 1;
  nr_of_blocks = (end_addr - start_addr + 1) / erase_block_size;
  
  /* erase all blocks one by one */
  for (block_cnt=0; block_cnt<nr_of_blocks; block_cnt++)
  {
    /* keep the watchdog happy */
    CopService();
    /* erase the block */
    if (MSC_ErasePage((uint32_t *)(start_addr + (block_cnt * erase_block_size))) != mscReturnOk)
    {
      /* error occurred during the erase operation */
      return BLT_FALSE;
    }
  }
  /* still here so all went okay */
  return BLT_TRUE;
} /*** end of FlashEraseSectors ***/


/****************************************************************************************
** NAME:           FlashGetSector
** PARAMETER:      address address in the flash sector
** RETURN VALUE:   flash sector number or FLASH_INVALID_SECTOR
** DESCRIPTION:    Determines the flash sector the address is in.
**
****************************************************************************************/
static blt_int8u FlashGetSector(blt_addr address)
{
  blt_int8u sectorIdx;
  
  /* search through the sectors to find the right one */
  for (sectorIdx = 0; sectorIdx < FLASH_TOTAL_SECTORS; sectorIdx++)
  {
    /* keep the watchdog happy */
    CopService();
    /* is the address in this sector? */
    if ( (address >= flashLayout[sectorIdx].sector_start) && \
         (address < (flashLayout[sectorIdx].sector_start + \
                  flashLayout[sectorIdx].sector_size)) )
    {
      /* return the sector number */
      return flashLayout[sectorIdx].sector_num;
    }
  }
  /* still here so no valid sector found */
  return FLASH_INVALID_SECTOR;
} /*** end of FlashGetSector ***/


/****************************************************************************************
** NAME:           FlashGetSectorBaseAddr
** PARAMETER:      sector sector to get the base address of.
** RETURN VALUE:   flash sector base address or FLASH_INVALID_ADDRESS
** DESCRIPTION:    Determines the flash sector base address.
**
****************************************************************************************/
static blt_addr FlashGetSectorBaseAddr(blt_int8u sector)
{
  blt_int8u sectorIdx;
  
  /* search through the sectors to find the right one */
  for (sectorIdx = 0; sectorIdx < FLASH_TOTAL_SECTORS; sectorIdx++)
  {
    /* keep the watchdog happy */
    CopService();
    if (flashLayout[sectorIdx].sector_num == sector)
    {
      return flashLayout[sectorIdx].sector_start;
    }
  }
  /* still here so no valid sector found */
  return FLASH_INVALID_ADDRESS;
} /*** end of FlashGetSectorBaseAddr ***/


/****************************************************************************************
** NAME:           FlashGetSectorSize
** PARAMETER:      sector sector to get the size of.
** RETURN VALUE:   flash sector size or 0
** DESCRIPTION:    Determines the flash sector size.
**
****************************************************************************************/
static blt_addr FlashGetSectorSize(blt_int8u sector)
{
  blt_int8u sectorIdx;
  
  /* search through the sectors to find the right one */
  for (sectorIdx = 0; sectorIdx < FLASH_TOTAL_SECTORS; sectorIdx++)
  {
    /* keep the watchdog happy */
    CopService();
    if (flashLayout[sectorIdx].sector_num == sector)
    {
      return flashLayout[sectorIdx].sector_size;
    }
  }
  /* still here so no valid sector found */
  return 0;
} /*** end of FlashGetSectorSize ***/


/*********************************** end of flash.c ************************************/