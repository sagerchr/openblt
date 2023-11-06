/**
 * \file IfxAsclin_Asc.c
 * \brief ASCLIN ASC details
 *
 * \version iLLD_1_0_1_16_1
 * \copyright Copyright (c) 2022 Infineon Technologies AG. All rights reserved.
 *
 *
 *
 *                                 IMPORTANT NOTICE
 *
 * Use of this file is subject to the terms of use agreed between (i) you or
 * the company in which ordinary course of business you are acting and (ii)
 * Infineon Technologies AG or its licensees. If and as long as no such terms
 * of use are agreed, use of this file is subject to following:
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer, must
 * be included in all copies of the Software, in whole or in part, and all
 * derivative works of the Software, unless such copies or derivative works are
 * solely in the form of machine-executable object code generated by a source
 * language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 *
 */

/******************************************************************************/
/*----------------------------------Includes----------------------------------*/
/******************************************************************************/

#include "IfxAsclin_Asc.h"
#include "string.h"

/******************************************************************************/
/*-------------------------Function Implementations---------------------------*/
/******************************************************************************/

uint8 IfxAsclin_Asc_blockingRead(IfxAsclin_Asc *asclin)
{
    Ifx_SizeT count = 1;
    uint8     data;

    while (IfxAsclin_Asc_read(asclin, &data, &count, TIME_INFINITE) != TRUE)
    {}

    return data;
}


boolean IfxAsclin_Asc_blockingWrite(IfxAsclin_Asc *asclin, uint8 data)
{
    Ifx_SizeT count = 1;

    return IfxAsclin_Asc_write(asclin, &data, &count, TIME_INFINITE);
}


boolean IfxAsclin_Asc_canReadCount(IfxAsclin_Asc *asclin, Ifx_SizeT count, Ifx_TickTime timeout)
{
    return Ifx_Fifo_canReadCount(asclin->rx, count, timeout);
}


boolean IfxAsclin_Asc_canWriteCount(IfxAsclin_Asc *asclin, Ifx_SizeT count, Ifx_TickTime timeout)
{
    return Ifx_Fifo_canWriteCount(asclin->tx, count, timeout);
}


void IfxAsclin_Asc_clearRx(IfxAsclin_Asc *asclin)
{
    IfxAsclin_flushRxFifo(asclin->asclin);
    Ifx_Fifo_clear(asclin->rx);
}


void IfxAsclin_Asc_clearTx(IfxAsclin_Asc *asclin)
{
    Ifx_Fifo_clear(asclin->tx);
    IfxAsclin_flushTxFifo(asclin->asclin);
}


void IfxAsclin_Asc_disableModule(IfxAsclin_Asc *asclin)
{
    Ifx_ASCLIN *asclinSFR = asclin->asclin;       /* getting the pointer to ASCLIN registers from module handler*/
    uint16      psw       = IfxScuWdt_getCpuWatchdogPassword();
    IfxScuWdt_clearCpuEndinit(psw);               /* clearing the endinit protection */
    IfxAsclin_setDisableModuleRequest(asclinSFR); /* disabling the module */
    IfxScuWdt_setCpuEndinit(psw);                 /* setting the endinit protection back on */
}


boolean IfxAsclin_Asc_flushTx(IfxAsclin_Asc *asclin, Ifx_TickTime timeout)
{
    Ifx_TickTime deadline = IfxStm_getDeadLine(timeout);
    boolean      result;

    /* Flush the software FIFO */
    result = Ifx_Fifo_flush(asclin->tx, timeout);

    if (result)
    {
        /* Flush the hardware FIFO (wait until all bytes have been transmitted) */
        do
        {
            result = IfxAsclin_getTxFifoFillLevel(asclin->asclin) == 0;
        } while (!result && !IfxStm_isDeadLine(deadline));
    }

    return result;
}


sint32 IfxAsclin_Asc_getReadCount(IfxAsclin_Asc *asclin)
{
    return Ifx_Fifo_readCount(asclin->rx);
}


IfxStdIf_DPipe_ReadEvent IfxAsclin_Asc_getReadEvent(IfxAsclin_Asc *asclin)
{
    return &asclin->rx->eventWriter;
}


uint32 IfxAsclin_Asc_getSendCount(IfxAsclin_Asc *asclin)
{
    return asclin->sendCount;
}


Ifx_TickTime IfxAsclin_Asc_getTxTimeStamp(IfxAsclin_Asc *asclin)
{
    return asclin->txTimestamp;
}


sint32 IfxAsclin_Asc_getWriteCount(IfxAsclin_Asc *asclin)
{
    return Ifx_Fifo_writeCount(asclin->tx);
}


IfxStdIf_DPipe_WriteEvent IfxAsclin_Asc_getWriteEvent(IfxAsclin_Asc *asclin)
{
    return &asclin->tx->eventWriter;
}


IfxAsclin_Status IfxAsclin_Asc_initModule(IfxAsclin_Asc *asclin, const IfxAsclin_Asc_Config *config)
{
    Ifx_ASCLIN      *asclinSFR = config->asclin;                        /* pointer to ASCLIN registers*/
    IfxAsclin_Status status    = IfxAsclin_Status_noError;

    asclin->asclin = asclinSFR;                                         /* adding register pointer to module handler*/

    IfxAsclin_enableModule(asclinSFR);                                  /* enabling the module*/
    IfxAsclin_setClockSource(asclinSFR, IfxAsclin_ClockSource_noClock); /* disabling the clock*/
    IfxAsclin_setFrameMode(asclinSFR, IfxAsclin_FrameMode_initialise);  /* setting the module in Initialise mode*/
    IfxAsclin_setPrescaler(asclinSFR, config->baudrate.prescaler);      /* sets the prescaler */
    IfxAsclin_setClockSource(asclinSFR, config->clockSource);           /* temporary set the clock source for baudrate configuration*/
    status = (IfxAsclin_Status)IfxAsclin_setBitTiming(asclinSFR,        /* setting the baudrate bit fields to generate the required baudrate*/
        config->baudrate.baudrate,
        config->baudrate.oversampling,
        config->bitTiming.samplePointPosition,
        config->bitTiming.medianFilter);
    IfxAsclin_setClockSource(asclinSFR, IfxAsclin_ClockSource_noClock);              /* disabling the clock again*/

    IfxAsclin_enableLoopBackMode(asclinSFR, config->loopBack);                       /* selecting the loopback mode */
    IfxAsclin_enableParity(asclinSFR, config->frame.parityBit);                      /* setting parity enable */
    IfxAsclin_setParityType(asclinSFR, config->frame.parityType);                    /* setting parity type (odd/even)*/
    IfxAsclin_setStopBit(asclinSFR, config->frame.stopBit);                          /* setting the stop bit */
    IfxAsclin_setShiftDirection(asclinSFR, config->frame.shiftDir);                  /* setting the shift direction */
    IfxAsclin_setDataLength(asclinSFR, config->frame.dataLength);                    /* setting the data length */
    IfxAsclin_setTxFifoInletWidth(asclinSFR, config->fifo.inWidth);                  /* setting Tx FIFO inlet width */
    IfxAsclin_setRxFifoOutletWidth(asclinSFR, config->fifo.outWidth);                /* setting Rx FIFO outlet width */
    IfxAsclin_setIdleDelay(asclinSFR, config->frame.idleDelay);                      /* setting idle delay */
    IfxAsclin_setTxFifoInterruptLevel(asclinSFR, config->fifo.txFifoInterruptLevel); /* setting Tx FIFO level at which a Tx interrupt will be triggered*/
    IfxAsclin_setRxFifoInterruptLevel(asclinSFR, config->fifo.rxFifoInterruptLevel); /* setting Rx FIFO interrupt level at which a Rx interrupt will be triggered*/
    IfxAsclin_setFrameMode(asclinSFR, config->frame.frameMode);                      /* selecting the frame mode*/

    /* Pin mapping */
    const IfxAsclin_Asc_Pins *pins = config->pins;

    if (pins != NULL_PTR)
    {
        IfxAsclin_Cts_In *cts = pins->cts;

        if (cts != NULL_PTR)
        {
            IfxAsclin_initCtsPin(cts, pins->ctsMode, pins->pinDriver);
        }

        IfxAsclin_Rx_In *rx = pins->rx;

        if (rx != NULL_PTR)
        {
            IfxAsclin_initRxPin(rx, pins->rxMode, pins->pinDriver);
        }

        IfxAsclin_Rts_Out *rts = pins->rts;

        if (rts != NULL_PTR)
        {
            IfxAsclin_initRtsPin(rts, pins->rtsMode, pins->pinDriver);
        }

        IfxAsclin_Tx_Out *tx = pins->tx;

        if (tx != NULL_PTR)
        {
            IfxAsclin_initTxPin(tx, pins->txMode, pins->pinDriver);
        }
    }

    IfxAsclin_setClockSource(asclinSFR, config->clockSource); /* select the clock source*/

    IfxAsclin_disableAllFlags(asclinSFR);                     /* disable all flags */
    IfxAsclin_clearAllFlags(asclinSFR);                       /* clear all flags */

    /* HW error flags */
    asclin->errorFlags.ALL = 0;

    if (config->errorFlags.flags.parityError)
    {
        IfxAsclin_enableParityErrorFlag(asclinSFR, TRUE);
    }

    if (config->errorFlags.flags.frameError)
    {
        IfxAsclin_enableFrameErrorFlag(asclinSFR, TRUE);
    }

    if (config->errorFlags.flags.rxFifoOverflow)
    {
        IfxAsclin_enableRxFifoOverflowFlag(asclinSFR, TRUE);
    }

    if (config->errorFlags.flags.rxFifoUnderflow)
    {
        IfxAsclin_enableRxFifoUnderflowFlag(asclinSFR, TRUE);
    }

    if (config->errorFlags.flags.txFifoOverflow)
    {
        IfxAsclin_enableTxFifoOverflowFlag(asclinSFR, TRUE);
    }

    /* transmission flags */
    asclin->rxSwFifoOverflow = FALSE;
    asclin->txInProgress     = FALSE;

    /* Buffer mode */
    Ifx_SizeT elementSize;
    asclin->dataBufferMode = config->dataBufferMode;
    asclin->txTimestamp    = 0;
    asclin->sendCount      = 0;

    switch (asclin->dataBufferMode)
    {
    case Ifx_DataBufferMode_normal:
        elementSize = 1;
        break;
    case Ifx_DataBufferMode_timeStampSingle:
        elementSize = sizeof(Ifx_DataBufferMode_TimeStampSingle);
        break;
    default:
        elementSize = 0;
        IFX_ASSERT(IFX_VERBOSE_LEVEL_ERROR, FALSE);
        break;
    }

    /* SW Fifos */
    if (config->txBuffer != NULL_PTR)
    {
        asclin->tx = Ifx_Fifo_init(config->txBuffer, config->txBufferSize, elementSize);
    }
    else
    {
        asclin->tx = Ifx_Fifo_create(config->txBufferSize, elementSize);
    }

    if (config->rxBuffer != NULL_PTR)
    {
        asclin->rx = Ifx_Fifo_init(config->rxBuffer, config->rxBufferSize, elementSize);
    }
    else
    {
        asclin->rx = Ifx_Fifo_create(config->rxBufferSize, elementSize);
    }

    /* initialising the interrupts */
    IfxSrc_Tos tos = config->interrupt.typeOfService;

    if ((config->interrupt.rxPriority > 0) || (tos == IfxSrc_Tos_dma))
    {
        volatile Ifx_SRC_SRCR *src;
        src = IfxAsclin_getSrcPointerRx(asclinSFR);
        IfxSrc_init(src, tos, config->interrupt.rxPriority);
        IfxAsclin_enableRxFifoFillLevelFlag(asclinSFR, TRUE);
        IfxSrc_enable(src);
    }

    if ((config->interrupt.txPriority > 0) || (tos == IfxSrc_Tos_dma))
    {
        volatile Ifx_SRC_SRCR *src;
        src = IfxAsclin_getSrcPointerTx(asclinSFR);
        IfxSrc_init(src, tos, config->interrupt.txPriority);
        IfxAsclin_enableTxFifoFillLevelFlag(asclinSFR, TRUE);
        IfxSrc_enable(src);
    }

    if (config->interrupt.erPriority > 0) /*These interrupts are not serviced by dma*/
    {
        volatile Ifx_SRC_SRCR *src;
        src = IfxAsclin_getSrcPointerEr(asclinSFR);
        IfxSrc_init(src, tos, config->interrupt.erPriority);
        IfxAsclin_enableParityErrorFlag(asclinSFR, TRUE);
        IfxSrc_enable(src);
    }

    /* enable transfers */
    IfxAsclin_enableRxFifoInlet(asclinSFR, TRUE);  // enabling Rx FIFO for recieving
    IfxAsclin_enableTxFifoOutlet(asclinSFR, TRUE); // enabling Tx FIFO for transmitting

    IfxAsclin_flushRxFifo(asclinSFR);              // flushing Rx FIFO
    IfxAsclin_flushTxFifo(asclinSFR);              // flushing Tx FIFO

    return status;
}


void IfxAsclin_Asc_initModuleConfig(IfxAsclin_Asc_Config *config, Ifx_ASCLIN *asclin)
{
    config->asclin = asclin;

    /* loop back disabled */
    config->loopBack = FALSE;                                                  /* no loop back*/

    /* Default values for baudrate */
    config->clockSource           = IfxAsclin_ClockSource_kernelClock;         /* kernel clock, fclc*/
    config->baudrate.prescaler    = 1;                                         /* default prescaler*/
    config->baudrate.baudrate     = 115200;                                    /* default baudrate (the fractional dividier setup will be calculated in initModule*/
    config->baudrate.oversampling = IfxAsclin_OversamplingFactor_4;            /* default oversampling factor*/

    /* Default Values for Bit Timings */
    config->bitTiming.medianFilter        = IfxAsclin_SamplesPerBit_one;       /* one sample per bit*/
    config->bitTiming.samplePointPosition = IfxAsclin_SamplePointPosition_3;   /* sample point position at 3*/
    /* Default Values for Frame Control */
    config->frame.idleDelay               = IfxAsclin_IdleDelay_0;             /* no idle delay*/
    config->frame.stopBit                 = IfxAsclin_StopBit_1;               /* one stop bit*/
    config->frame.frameMode               = IfxAsclin_FrameMode_asc;           /* ASC mode*/
    config->frame.shiftDir                = IfxAsclin_ShiftDirection_lsbFirst; /* shift diection LSB first*/
    config->frame.parityBit               = FALSE;                             /* disable parity*/
    config->frame.parityType              = IfxAsclin_ParityType_even;         /* even parity (if parity enabled)*/
    config->frame.dataLength              = IfxAsclin_DataLength_8;            /* number of bits per transfer 8*/

    /* Default Values for Fifo Control */
    config->fifo.inWidth              = IfxAsclin_TxFifoInletWidth_1;          /* 8-bit wide write*/
    config->fifo.outWidth             = IfxAsclin_RxFifoOutletWidth_1;         /* 8-bit wide read*/
    config->fifo.txFifoInterruptLevel = IfxAsclin_TxFifoInterruptLevel_0;      /* txFifoInterruptLevel = 0. optimised to write upto 16 bytes at a time */
    config->fifo.rxFifoInterruptLevel = IfxAsclin_RxFifoInterruptLevel_1;
    config->fifo.buffMode             = IfxAsclin_ReceiveBufferMode_rxFifo;    /* RxFIFO*/

    /* Default Values for Interrupt Config */
    config->interrupt.rxPriority    = 0;                                       /* receive interrupt priority 0*/
    config->interrupt.txPriority    = 0;                                       /* transmit interrupt priority 0*/
    config->interrupt.erPriority    = 0;                                       /* error interrupt priority 0*/
    config->interrupt.typeOfService = IfxSrc_Tos_cpu0;                         /* type of service CPU0*/

    /* Enable error flags */
    config->errorFlags.ALL = ~0;                                               /* all error flags enabled*/

    /* init pointers */
    config->pins           = NULL_PTR;                                         /* pins to null pointer*/
    config->rxBuffer       = NULL_PTR;                                         /* Rx Fifo buffer*/
    config->txBuffer       = NULL_PTR;                                         /* Tx Fifo buffer*/

    config->txBufferSize   = 0;                                                /* Rx Fifo buffer size*/
    config->rxBufferSize   = 0;                                                /* Rx Fifo buffer size*/

    config->dataBufferMode = Ifx_DataBufferMode_normal;
}


void IfxAsclin_Asc_initiateTransmission(IfxAsclin_Asc *asclin)
{
    if (asclin->txInProgress == FALSE)     /* Send first byte: send init */
    {
        if (Ifx_Fifo_isEmpty(asclin->tx) == FALSE)
        {
            uint8 data;
            asclin->txInProgress = TRUE;

            switch (asclin->dataBufferMode)
            {
            case Ifx_DataBufferMode_normal:
            {
                Ifx_Fifo_read(asclin->tx, &data, 1, TIME_NULL);
            }
            break;
            case Ifx_DataBufferMode_timeStampSingle:
            {
                Ifx_DataBufferMode_TimeStampSingle packedData;
                Ifx_Fifo_read(asclin->tx, &packedData, sizeof(packedData), TIME_NULL);
                data = packedData.data;
            }
            break;
            }

            IfxAsclin_write8(asclin->asclin, &data, 1);
        }
    }
}


void IfxAsclin_Asc_isrError(IfxAsclin_Asc *asclin)
{
    Ifx_ASCLIN *asclinSFR = asclin->asclin; /* getting the pointer to ASCLIN registers from module handler*/

    /* store all the flags in the variable */
    if (IfxAsclin_getParityErrorFlagStatus(asclinSFR))
    {
        IfxAsclin_clearParityErrorFlag(asclinSFR);
        asclin->errorFlags.flags.parityError = 1;
    }

    if (IfxAsclin_getFrameErrorFlagStatus(asclinSFR))
    {
        IfxAsclin_clearFrameErrorFlag(asclinSFR);
        asclin->errorFlags.flags.frameError = 1;
    }

    if (IfxAsclin_getRxFifoOverflowFlagStatus(asclinSFR))
    {
        IfxAsclin_clearRxFifoOverflowFlag(asclinSFR);
        asclin->errorFlags.flags.rxFifoOverflow = 1;
    }

    if (IfxAsclin_getRxFifoUnderflowFlagStatus(asclinSFR))
    {
        IfxAsclin_clearRxFifoUnderflowFlag(asclinSFR);
        asclin->errorFlags.flags.rxFifoUnderflow = 1;
    }

    if (IfxAsclin_getTxFifoOverflowFlagStatus(asclinSFR))
    {
        IfxAsclin_clearTxFifoOverflowFlag(asclinSFR);
        asclin->errorFlags.flags.txFifoOverflow = 1;
    }
}


void IfxAsclin_Asc_isrReceive(IfxAsclin_Asc *asclin)
{
    uint8 ascData[16]; /*FIFO size is 16 bytes*/

    switch (asclin->dataBufferMode)
    {
    case Ifx_DataBufferMode_normal:
    {
        uint8 count;
        count = IfxAsclin_getRxFifoFillLevel(asclin->asclin);
        IfxAsclin_read8(asclin->asclin, &ascData[0], count);

        if (Ifx_Fifo_write(asclin->rx, &ascData[0], count, TIME_NULL) != 0)
        {
            /* Receive buffer is full, data is discard */
            asclin->rxSwFifoOverflow = TRUE;
        }

        break;
    }
    case Ifx_DataBufferMode_timeStampSingle:
    {
        Ifx_DataBufferMode_TimeStampSingle packedData;

        while (IfxAsclin_getRxFifoFillLevel(asclin->asclin) > 0)
        {
            packedData.timestamp = IfxStm_now();
            IfxAsclin_read8(asclin->asclin, &ascData[0], 1);
            packedData.data      = ascData[0];

            if (Ifx_Fifo_write(asclin->rx, &packedData, sizeof(packedData), TIME_NULL) != 0)
            {
                /* Receive buffer is full, data is discard */
                asclin->rxSwFifoOverflow = TRUE;
            }
        }
    }
    break;
    }
}


void IfxAsclin_Asc_isrTransmit(IfxAsclin_Asc *asclin)
{
    asclin->txTimestamp = IfxStm_now();
    asclin->sendCount++;

    if (Ifx_Fifo_isEmpty(asclin->tx) == FALSE)
    {
        switch (asclin->dataBufferMode)
        {
        case Ifx_DataBufferMode_normal:
        {
            uint8          ascData[16];
            uint16         count            = 0, i_count = 0;
            volatile uint8 hw_tx_fill_level = 0;

            count            = Ifx_Fifo_readCount(asclin->tx); /*SW FIFO fill level*/

            hw_tx_fill_level = IfxAsclin_getTxFifoFillLevel(asclin->asclin);

            i_count          = (16 - hw_tx_fill_level);

            if (i_count > count)
            {
                i_count = count;
            }

            Ifx_Fifo_read(asclin->tx, &ascData[0], i_count, TIME_NULL);
            IfxAsclin_write8(asclin->asclin, &ascData[0], i_count);
        }
        break;

        case Ifx_DataBufferMode_timeStampSingle:
        {
            Ifx_DataBufferMode_TimeStampSingle packedData;
            uint8                              ascData;

            Ifx_Fifo_read(asclin->tx, &packedData, sizeof(packedData), TIME_NULL);

            ascData = packedData.data;
            IfxAsclin_write8(asclin->asclin, &ascData, 1);
        }
        break;
        }
    }
    else
    {
        /* Transmit buffer is empty */
        asclin->txInProgress = FALSE;
    }
}


boolean IfxAsclin_Asc_read(IfxAsclin_Asc *asclin, void *data, Ifx_SizeT *count, Ifx_TickTime timeout)
{
    Ifx_SizeT left = Ifx_Fifo_read(asclin->rx, data, *count, timeout);

    *count -= left;

    return left == 0;
}


void IfxAsclin_Asc_resetSendCount(IfxAsclin_Asc *asclin)
{
    asclin->sendCount = 0;
}


boolean IfxAsclin_Asc_stdIfDPipeInit(IfxStdIf_DPipe *stdif, IfxAsclin_Asc *asclin)
{
    /* Ensure the stdif is reset to zeros */
    memset(stdif, 0, sizeof(IfxStdIf_DPipe));

    /* Set the API link */
    stdif->driver         = asclin;
    stdif->write          = (IfxStdIf_DPipe_Write) & IfxAsclin_Asc_write;
    stdif->read           = (IfxStdIf_DPipe_Read) & IfxAsclin_Asc_read;
    stdif->getReadCount   = (IfxStdIf_DPipe_GetReadCount) & IfxAsclin_Asc_getReadCount;
    stdif->getReadEvent   = (IfxStdIf_DPipe_GetReadEvent) & IfxAsclin_Asc_getReadEvent;
    stdif->getWriteCount  = (IfxStdIf_DPipe_GetWriteCount) & IfxAsclin_Asc_getWriteCount;
    stdif->getWriteEvent  = (IfxStdIf_DPipe_GetWriteEvent) & IfxAsclin_Asc_getWriteEvent;
    stdif->canReadCount   = (IfxStdIf_DPipe_CanReadCount) & IfxAsclin_Asc_canReadCount;
    stdif->canWriteCount  = (IfxStdIf_DPipe_CanWriteCount) & IfxAsclin_Asc_canWriteCount;
    stdif->flushTx        = (IfxStdIf_DPipe_FlushTx) & IfxAsclin_Asc_flushTx;
    stdif->clearTx        = (IfxStdIf_DPipe_ClearTx) & IfxAsclin_Asc_clearTx;
    stdif->clearRx        = (IfxStdIf_DPipe_ClearRx) & IfxAsclin_Asc_clearRx;
    stdif->onReceive      = (IfxStdIf_DPipe_OnReceive) & IfxAsclin_Asc_isrReceive;
    stdif->onTransmit     = (IfxStdIf_DPipe_OnTransmit) & IfxAsclin_Asc_isrTransmit;
    stdif->onError        = (IfxStdIf_DPipe_OnError) & IfxAsclin_Asc_isrError;
    stdif->getSendCount   = (IfxStdIf_DPipe_GetSendCount) & IfxAsclin_Asc_getSendCount;
    stdif->getTxTimeStamp = (IfxStdIf_DPipe_GetTxTimeStamp) & IfxAsclin_Asc_getTxTimeStamp;
    stdif->resetSendCount = (IfxStdIf_DPipe_ResetSendCount) & IfxAsclin_Asc_resetSendCount;
    stdif->txDisabled     = FALSE;
    return TRUE;
}


boolean IfxAsclin_Asc_write(IfxAsclin_Asc *asclin, const void *data, Ifx_SizeT *count, Ifx_TickTime timeout)
{
    Ifx_SizeT left;
    boolean   result       = TRUE;
    boolean   tx_status    = asclin->txInProgress;
    uint16    tx_fifo_size = (uint16)asclin->tx->size;

    if (*count != 0)
    {
        if ((!tx_status) && (*count > tx_fifo_size))                 /*Transmission not initialised and data size is greater than S/W FIFO*/
        {
            Ifx_Fifo_write(asclin->tx, data, tx_fifo_size, timeout); /*Fill upto the S/W FIFO size and initiate transmission*/
            IfxAsclin_Asc_initiateTransmission(asclin);

            left = Ifx_Fifo_write(asclin->tx, (((const uint8 *)data) + tx_fifo_size), (*count) - tx_fifo_size, timeout);
            IfxAsclin_Asc_initiateTransmission(asclin); /*Will initiate transmission only if txInProgress is False, otherwise interrupts will continue to push data to H/W FIFO*/
        }

        else
        {
            left = Ifx_Fifo_write(asclin->tx, data, *count, timeout);
            IfxAsclin_Asc_initiateTransmission(asclin);
        }

        *count -= left;
        result  = left == 0;
    }

    return result;
}
