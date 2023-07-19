/*
 * Serial/USB MIDI player for CH32V003/V203
 *
 *  for 003
 *  Input: USART1 RX (PD6)
 *  Output: TIM1CH4 (PC4)
 *
 *  for 203
 *  Input: USART1 RX (PA11)
 *   or  : USB-OTG (PB6/7)
 *  Output: TIM2CH4/3 (PA2/3)
 */

#include "debug.h"

#define TIME_UNIT 100000000                           // Oscillator calculation resolution = 10nsec
#define SAMPLING_INTERVAL (TIME_UNIT/SAMPLING_FREQ)   // 20.833 usec in 48KHz Sampling freq
#define RX_BUFFER_LEN 256

// Choose Serial interface

#define SERIAL_SPEED 115200                            // USB-Serial Bridge
//#define SERIAL_SPEED 31250                             // MIDI interface(31.25Kbps)

#ifdef CH32V20x_D6
// USB MIDI on USB-OTG interface
// if you want to disable this option, please exclude USB_Device folder in build setting.
#define USE_USB_MIDI
#endif

#ifdef USE_USB_MIDI
#include "ch32v20x_usbfs_device.h"
#include "usbmidi.h"

__attribute__ ((aligned(4))) uint8_t midi_buff[DEF_USBD_FS_PACK_SIZE * MIDI_RX_PACKETS ];
volatile uint8_t midi_packetlen[MIDI_RX_PACKETS];
volatile uint32_t midi_load_count = 0;
volatile uint32_t midi_remain_count = 0;
volatile uint8_t midi_stop_flag = 0;

#define GETCH() usb_getch()
#else
#define GETCH() usart_getch()
#endif

#ifdef CH32V20x_D6
#define SAMPLING_FREQ 48000
#define OUTPUT_CHANNELS 2
#define PSG_NUMBER 6
#define PSG_DEVIDE_FACTOR 11
#else
#define SAMPLING_FREQ 32000
#define OUTPUT_CHANNELS 1                               // should be 1 for CH32V003
#define PSG_NUMBER 4
#define PSG_DEVIDE_FACTOR 9
#endif

//#define SAMPLING_FREQ 48000
//#define SAMPLING_FREQ 44100
//#define SAMPLING_FREQ 32000
//#define SAMPLING_FREQ 22000

// Select Voices

//#define PSG_NUMBER 1
//#define PSG_DEVIDE_FACTOR 3
//#define PSG_NUMBER 2
//#define PSG_DEVIDE_FACTOR 5
//#define PSG_NUMBER 3
//#define PSG_DEVIDE_FACTOR 7
//#define PSG_NUMBER 4
//#define PSG_DEVIDE_FACTOR 9
//#define PSG_NUMBER 5
//#define PSG_DEVIDE_FACTOR 11
//#define PSG_NUMBER 6
//#define PSG_DEVIDE_FACTOR 11

//

#define PSG_NUMBERS (PSG_NUMBER *OUTPUT_CHANNELS)

//#define PSG_DEBUG

/* Global Variable */

uint32_t psg_osc_freq[3 * PSG_NUMBERS];
uint32_t psg_osc_interval[3 * PSG_NUMBERS];
uint32_t psg_osc_counter[3 * PSG_NUMBERS];
uint8_t psg_tone_volume[3 * PSG_NUMBERS];
uint8_t psg_tone_on[3 * PSG_NUMBERS];

uint16_t psg_master_volume[OUTPUT_CHANNELS];

uint8_t psg_midi_inuse[3 * PSG_NUMBERS];
uint8_t psg_midi_inuse_ch[3 * PSG_NUMBERS];
uint8_t psg_midi_note[3 * PSG_NUMBERS];

uint8_t midi_ch_volume[16];

// Volume table (LOG)
/*
 const uint16_t psg_volume[] = { 0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x04,
 0x05, 0x06, 0x07, 0x08, 0x09, 0x0b, 0x0d, 0x10, 0x13, 0x17, 0x1b, 0x20,
 0x26, 0x2d, 0x36, 0x40, 0x4c, 0x5a, 0x6b, 0x80, 0x98, 0xb4, 0xd6, 0xff };
 */

// Volume table (LINER)
const uint16_t psg_volume[] = { 0x00, 0x00, 0x17, 0x20, 0x27, 0x30, 0x37, 0x40,
        0x47, 0x50, 0x57, 0x60, 0x67, 0x70, 0x77, 0x80, 0x87, 0x90, 0x97, 0xa0,
        0xa7, 0xb0, 0xb7, 0xc0, 0xc7, 0xd0, 0xd7, 0xe0, 0xe7, 0xf0, 0xf7, 0xff };

//

const uint16_t midifreq[] = { 8, 9, 9, 10, 10, 11, 12, 12, 13, 14, 15, 15, 16,
        17, 18, 19, 21, 22, 23, 24, 26, 28, 29, 31, 33, 35, 37, 39, 41, 44, 46,
        49, 52, 55, 58, 62, 65, 69, 73, 78, 82, 87, 92, 98, 104, 110, 117, 123,
        131, 139, 147, 156, 165, 175, 185, 196, 208, 220, 233, 247, 262, 277,
        294, 311, 330, 349, 370, 392, 415, 440, 466, 494, 523, 554, 587, 622,
        659, 698, 740, 784, 831, 880, 932, 988, 1047, 1109, 1175, 1245, 1319,
        1397, 1480, 1568, 1661, 1760, 1865, 1976, 2093, 2217, 2349, 2489, 2637,
        2794, 2960, 3136, 3322, 3520, 3729, 3951, 4186, 4435, 4699, 4978, 5274,
        5588, 5920, 6272, 6645, 7040, 7459, 7902, 8372, 8870, 9397, 9956, 10548,
        11175, 11840, 12544 };

volatile uint8_t rxbuff[RX_BUFFER_LEN];
uint8_t rxptr = 0;
uint32_t lastptr = RX_BUFFER_LEN;

#ifdef CH32V20x_D6
void TIM2_PWMOut_Init(u16 arr, u16 psc, u16 ccp) {
    TIM_OCInitTypeDef TIM_OCInitStructure = { 0 };
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = { 0 };

    TIM_TimeBaseInitStructure.TIM_Period = arr;
    TIM_TimeBaseInitStructure.TIM_Prescaler = psc;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit( TIM2, &TIM_TimeBaseInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;

#if OUTPUT_CHANNELS>3
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = ccp;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init( TIM2, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig( TIM2, TIM_OCPreload_Disable);
#endif
#if OUTPUT_CHANNELS>2
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = ccp;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC2Init( TIM2, &TIM_OCInitStructure);
    TIM_OC2PreloadConfig( TIM2, TIM_OCPreload_Disable);
#endif
#if OUTPUT_CHANNELS>1
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = ccp;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC3Init( TIM2, &TIM_OCInitStructure);
    TIM_OC3PreloadConfig( TIM2, TIM_OCPreload_Disable);
#endif
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = ccp;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC4Init( TIM2, &TIM_OCInitStructure);
    TIM_OC4PreloadConfig( TIM2, TIM_OCPreload_Disable);

    TIM_CtrlPWMOutputs(TIM2, ENABLE);
    TIM_ARRPreloadConfig( TIM2, ENABLE);
    TIM_Cmd( TIM2, ENABLE);
}
#else
void TIM1_PWMOut_Init(u16 arr, u16 psc, u16 ccp) {
    TIM_OCInitTypeDef TIM_OCInitStructure = {0};
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = {0};

    TIM_TimeBaseInitStructure.TIM_Period = arr;
    TIM_TimeBaseInitStructure.TIM_Prescaler = psc;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit( TIM1, &TIM_TimeBaseInitStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;

    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = ccp;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC4Init( TIM1, &TIM_OCInitStructure);

    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_OC4PreloadConfig( TIM1, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig( TIM1, ENABLE);
    TIM_Cmd( TIM1, ENABLE);
}
#endif

void toneinit(void) {

    GPIO_InitTypeDef GPIO_InitStructure = { 0 };

#ifdef CH32V20x_D6
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd( RCC_APB1Periph_TIM2, ENABLE);

#if OUTPUT_CHANNELS>3
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOA, &GPIO_InitStructure);
#endif

#if OUTPUT_CHANNELS>2
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOA, &GPIO_InitStructure);
#endif

#if OUTPUT_CHANNELS>1
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOA, &GPIO_InitStructure);
#endif

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOA, &GPIO_InitStructure);

#else
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOC | RCC_APB2Periph_TIM1, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOC, &GPIO_InitStructure);
#endif

#ifdef PSG_DEBUG
#ifdef CH32V20x_D6
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;    // for debug
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOA, &GPIO_InitStructure);
#else
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;    // for debug
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOC, &GPIO_InitStructure);
#endif
#endif

}

void psg_reset(void) {

    for (int i = 0; i < 3 * PSG_NUMBERS; i++) {
        psg_osc_interval[i] = UINT32_MAX;
        psg_osc_counter[i] = 0;
        psg_osc_freq[i] = 0;
        psg_tone_on[i] = 0;
        psg_tone_volume[i] = 0;
        psg_midi_inuse[i]=0;
    }
}

static inline void noteon(uint8_t i, uint8_t note, uint8_t volume) {
    uint32_t freq;

    freq = midifreq[note];

    psg_osc_interval[i] = TIME_UNIT / freq;
    psg_osc_counter[i] = 0;
    psg_tone_volume[i] = volume;
    psg_tone_on[i] = 1;

}

static inline void noteoff(uint8_t i, uint8_t note) {
    psg_osc_interval[i] = UINT32_MAX;
    psg_osc_freq[i] = 0;
    psg_tone_on[i] = 0;
}

void usart_init(uint32_t baudrate) {
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

#ifdef CH32V20x_D6
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1,
            ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
#else
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1,
            ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
#endif

#ifdef PSG_DEBUG
#ifdef CH32V20x_D6
#else
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
#endif
#endif

    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl =
    USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(USART1, &USART_InitStructure);
    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);

    USART_Cmd(USART1, ENABLE);

}

void DMA_Rx_Init(DMA_Channel_TypeDef *DMA_CHx, u32 ppadr, u32 memadr,
        u16 bufsize) {
    DMA_InitTypeDef DMA_InitStructure = { 0 };

    RCC_AHBPeriphClockCmd( RCC_AHBPeriph_DMA1, ENABLE);

    DMA_DeInit(DMA_CHx);

    DMA_InitStructure.DMA_PeripheralBaseAddr = ppadr;
    DMA_InitStructure.DMA_MemoryBaseAddr = memadr;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = bufsize;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA_CHx, &DMA_InitStructure);

    DMA_Cmd(DMA_CHx, ENABLE);

}

#ifndef USE_USB_MIDI
static inline uint8_t usart_getch() {

    uint8_t ch;
    uint32_t currptr;

    currptr = DMA_GetCurrDataCounter(DMA1_Channel5);

    while(1) {
        currptr=DMA_GetCurrDataCounter(DMA1_Channel5);
        if(currptr!=lastptr) break;

    }

    ch = rxbuff[rxptr];
    lastptr--;
    if (lastptr == 0) {
        lastptr = RX_BUFFER_LEN;
    }

    rxptr++;
    if (rxptr >= RX_BUFFER_LEN)
        rxptr = 0;

    return ch;

}
#endif

#ifdef USE_USB_MIDI
static inline uint8_t usb_getch() {

    static uint8_t midi_read_count = MIDI_RX_PACKETS;
    static uint8_t midi_read_ptr = 0;
    static uint8_t midi_ptr = 0;
    static uint8_t midi_data[4];
    uint8_t ch, load_flag;

    restart: if (midi_ptr != 0) {
        midi_ptr++;
        ch = midi_data[midi_ptr];
        if (midi_ptr == 3)
            midi_ptr = 0;
        return ch;
    }

    load_flag = 0;
#ifdef PSG_DEBUG
    printf("USB:packets %d %d %d %d/%d\n\r",midi_load_count,midi_read_count,midi_remain_count,midi_read_ptr,midi_packetlen[midi_read_count]);
#endif
    //   if(midi_remain_count==0) {  // check last packet
    if (midi_read_ptr >= midi_packetlen[midi_read_count]) {  // shoud load next packet
        load_flag = 1;
    } else {
        ch = midi_buff[DEF_USBD_FS_PACK_SIZE * midi_read_count + midi_read_ptr];
        if (ch == 0)
            load_flag = 1;
    }
//    }
#ifdef PSG_DEBUG
    printf("USB:load_flag %d %d %x\n\r",load_flag,ch,USBOTG_FS->UEP2_RX_CTRL);
#endif
    if (load_flag == 1) {
        midi_read_ptr = 0;
        while(load_flag==1) {
            while(midi_remain_count==0);
            midi_read_count++;
#ifdef PSG_DEBUG
            printf("USB:packets %d %d %d %d/%d\n\r",midi_load_count,midi_read_count,midi_remain_count,midi_read_ptr,midi_packetlen[midi_read_count]);
#endif
            if (midi_read_count >= MIDI_RX_PACKETS)
                midi_read_count = 0;
            midi_remain_count--;
            if(midi_packetlen[midi_read_count]>=4) load_flag=0;
        }

        if (midi_stop_flag == 1) {
            midi_stop_flag = 0;
            USBOTG_FS->UEP2_RX_CTRL &= ~USBFS_UEP_R_RES_MASK;
            USBOTG_FS->UEP2_RX_CTRL |= USBFS_UEP_R_RES_ACK;
        }
    }

    for (int i = 0; i < 4; i++) {
        midi_data[i] = midi_buff[DEF_USBD_FS_PACK_SIZE * midi_read_count
                + midi_read_ptr + i];
    }

    midi_read_ptr += 4;

    if ((midi_data[0] & 0xf0) != 0) {

        goto restart;
    }
    midi_ptr = 1;

    return midi_data[1];

}
#endif

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void) {

    uint8_t midicmd, midich, midicc1, midicc2, midinote, midivel;
    uint8_t override;
    NVIC_InitTypeDef NVIC_InitStructure = { 0 };

    // Intialize systick interrupt by sampling frequency

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

#ifdef USE_USB_MIDI
    //   RCC_Configuration();
    USBFS_RCC_Init();
    USBFS_Device_Init(ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = SysTicK_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = USBHD_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

#endif

    // NVIC_SetPriority(SysTicK_IRQn, 0);
    NVIC_EnableIRQ(SysTicK_IRQn);
    SysTick->SR &= ~(1 << 0);
#ifdef CH32V20x_D6
    SysTick->CMP = (uint64_t) ((SystemCoreClock / SAMPLING_FREQ) - 1);
#else
    SysTick->CMP = (SystemCoreClock / SAMPLING_FREQ) - 1;
#endif
    SysTick->CNT = 0;
    SysTick->CTLR = 0xF;

#ifdef PSG_DEBUG
    USART_Printf_Init(115200);
    printf("SystemClk:%d\r\n", SystemCoreClock);
#endif

    usart_init(SERIAL_SPEED);

    DMA_Rx_Init( DMA1_Channel5, (u32) &USART1->DATAR, (u32) &rxbuff,
    RX_BUFFER_LEN);

    toneinit();
#ifdef CH32V20x_D6
    TIM2_PWMOut_Init(256, 0, 255);  // 48MHz * 256 = 5us
#else
            TIM1_PWMOut_Init(256, 0, 255);  // 48MHz * 256 = 5us
#endif

    // Initialize PSG

    //   while(1) {
    //      printf("%x ",usart_getch());
    //  }

    for (int i = 0; i < OUTPUT_CHANNELS; i++) {
        psg_master_volume[i] = 0;
    }
    psg_reset();

    while(1) {

        // Listen USART

        midicmd=GETCH();
        midich=midicmd&0xf;

#ifdef PSG_DEBUG

        printf("CMD:%02x\n\r",midicmd);

#endif

        switch(midicmd&0xf0) {
            case 0x80: // Note off

            midinote=GETCH();
            midivel=GETCH();

#ifdef PSG_DEBUG
            printf("Note off: %d %d %d\n\r",midich,midinote,midivel);
#endif
            for(int i=0; i< 3*PSG_NUMBERS;i++) {
                if((psg_midi_inuse[i]==1)&&(psg_midi_inuse_ch[i]==midich)&&(psg_midi_note[i]==midinote)) {
                    noteoff(i,midinote);
                    psg_midi_inuse[i]=0;
                    //                   break;
                }
            }

            break;

            case 0x90: // Note on

            midinote=GETCH();
            midivel=GETCH();

            if(midich!=9) {
#ifdef PSG_DEBUG
                printf("Note on: %d %d %d\n\r",midich,midinote,midivel);
#endif
                if(midivel!=0) {
                    // check note is already on
                    override=0;
                    for(int i=0; i< 3*PSG_NUMBERS;i++) {
                        if((psg_midi_inuse[i]==1)&&(psg_midi_inuse_ch[i]==midich)&&(psg_midi_note[i]==midinote)) {
#ifdef PSG_DEBUG
                            printf("PSG override: %d\n\r",i);
#endif
                            override=1;
                            //                           break;
                        }
                    }

                    if(override==0) {
                        for(int i=0; i< 3*PSG_NUMBERS;i++) {
                            if(psg_midi_inuse[i]==0) {
                                noteon(i,midinote,midi_ch_volume[midich]);
                                psg_midi_inuse[i]=1;
                                psg_midi_inuse_ch[i]=midich;
                                psg_midi_note[i]=midinote;
#ifdef PSG_DEBUG
                                printf("PSG on: %d\n\r",i);

                                for(int j=0;j<3*PSG_NUMBERS;j++) {
                                    printf("%x:%x:%x ",psg_midi_inuse[j],psg_midi_inuse_ch[j],psg_midi_note[j]);
                                }
                                printf("\n\r");

#endif
                                break;
                            }
                        }
                    }
                } else {
                    for(int i=0; i< 3*PSG_NUMBERS;i++) {
                        if((psg_midi_inuse[i]==1)&&(psg_midi_inuse_ch[i]==midich)&&(psg_midi_note[i]==midinote)) {
                            noteoff(i,midinote);
                            psg_midi_inuse[i]=0;
#ifdef PSG_DEBUG
                            printf("PSG off: %d\n\r",i);
#endif
//                            break;
                        }
                    }
                }
            }
            break;

            case 0xb0:
            // Channel control

            midicc1=GETCH();

            switch(midicc1) {
                case 7:
                case 11: // Expression
                midicc2=GETCH();
#ifdef PSG_DEBUG
                printf("Expression: %d %d\n\r",midich,midicc2);
#endif
                midi_ch_volume[midich]=(midicc2>>3);
                break;

                case 0: //Bank select
                case 120:// All note off
                case 121:// All reset
                case 123:
                case 124:
                case 125:
                case 126:
                case 127:
                for(int i=0;i<3*PSG_NUMBERS;i++) {
                    if((psg_midi_inuse[i]==1)&&(psg_midi_inuse_ch[i]==midich)) {
                        noteoff(i,psg_midi_note[i]);
                        psg_midi_inuse[i]=0;
                    }
                }
                break;

                default:
                break;

            }

            break;
            case 0xc0:

            // Program change
            for(int i=0;i<3*PSG_NUMBERS;i++) {
                if((psg_midi_inuse[i]==1)&&(psg_midi_inuse_ch[i]==midich)) {
                    noteoff(i,psg_midi_note[i]);
                    psg_midi_inuse[i]=0;
                }
            }

            break;
            default: // Skip
            break;

        }
    }
}

void SysTick_Handler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
//void SysTick_Handler(void) __attribute__((interrupt));

void SysTick_Handler(void) {

    uint32_t pon_count;
    uint16_t master_volume[OUTPUT_CHANNELS];
    uint8_t tone_output[3 * PSG_NUMBERS];
    uint8_t enable_channels;

#ifdef CH32V20x_D6
    TIM2->CH4CVR = psg_master_volume[0];
#if OUTPUT_CHANNELS>1
    TIM2->CH3CVR = psg_master_volume[1];
#endif
#if OUTPUT_CHANNELS>2
    TIM2->CH2CVR = psg_master_volume[2];
#endif
#if OUTPUT_CHANNELS>3
    TIM2->CH1CVR = psg_master_volume[3];
#endif

#else   //CH32V003
    TIM1->CH4CVR = psg_master_volume[0];
#endif

#ifdef PSG_DEBUG
#ifdef CH32V20x_D6
    GPIO_WriteBit(GPIOA, GPIO_Pin_4, Bit_SET);
#else
    GPIO_WriteBit(GPIOC, GPIO_Pin_3, Bit_SET);
#endif
#endif

// Run Oscillator

    for (int i = 0; i < 3 * PSG_NUMBERS; i++) {
        pon_count = psg_osc_counter[i] += SAMPLING_INTERVAL;
        if (pon_count < (psg_osc_interval[i] / 2)) {
            tone_output[i] = psg_tone_on[i];
        } else if (pon_count > psg_osc_interval[i]) {
            psg_osc_counter[i] -= psg_osc_interval[i];
            tone_output[i] = psg_tone_on[i];
        } else {
            tone_output[i] = 0;
        }
    }

// Mixer

    for (int i = 0; i < OUTPUT_CHANNELS; i++) {
        master_volume[i] = 0;
    }

    enable_channels = 0;

    for (int i = 0; i < 3 * PSG_NUMBERS; i++) {
        if (psg_tone_on[i] == 1) {
            if (tone_output[i] != 0) {
                master_volume[i % OUTPUT_CHANNELS] +=
                        psg_volume[midi_ch_volume[psg_midi_inuse_ch[i]] * 2 + 1];

            }
            enable_channels++;
        }
    }

    for (int i = 0; i < OUTPUT_CHANNELS; i++) {
        psg_master_volume[i] = master_volume[i] / PSG_DEVIDE_FACTOR;

        if (psg_master_volume[i] > 255)
            psg_master_volume[i] = 255;

    }

    SysTick->SR &= 0;

#ifdef PSG_DEBUG
#ifdef CH32V20x_D6
    GPIO_WriteBit(GPIOA, GPIO_Pin_4, Bit_RESET);
#else
    GPIO_WriteBit(GPIOC, GPIO_Pin_3, Bit_RESET);
#endif
#endif

}
