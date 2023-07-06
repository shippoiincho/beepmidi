/*
 * Serial MIDI player for CH32V003
 * Input I2C (SDA PC1/SCL PC2)
 *  Output: TIM1CH4 (PC4)
 */

#include "debug.h"
//#define SAMPLING_FREQ 48000
//#define SAMPLING_FREQ 44100
#define SAMPLING_FREQ 32000
//#define SAMPLING_FREQ 22000
#define TIME_UNIT 100000000                           // Oscillator calculation resolution = 10nsec
#define SAMPLING_INTERVAL (TIME_UNIT/SAMPLING_FREQ)   // 20.833 usec in 48KHz Sampling freq
#define RX_BUFFER_LEN 256

// Choose Serial interface

#define SERIAL_SPEED 115200                            // USB-Serial Bridge
//#define SERIAL_SPEED 31250                             // MIDI interface(31.25Kbps)

//#define PSG_NUMBERS 1
//#define PSG_DEVIDE_FACTOR 3
//#define PSG_NUMBERS 2
//#define PSG_DEVIDE_FACTOR 5
//#define PSG_NUMBERS 3
//#define PSG_DEVIDE_FACTOR 7
#define PSG_NUMBERS 4
#define PSG_DEVIDE_FACTOR 9
//#define PSG_NUMBERS 5
//#define PSG_DEVIDE_FACTOR 11

//#define PSG_DEBUG

/* Global Variable */

uint32_t psg_osc_freq[3 * PSG_NUMBERS];
uint32_t psg_osc_interval[3 * PSG_NUMBERS];
uint32_t psg_osc_counter[3 * PSG_NUMBERS];
uint8_t psg_tone_volume[3 * PSG_NUMBERS];
uint8_t psg_tone_on[3 * PSG_NUMBERS];

uint16_t psg_master_volume = 0;

uint8_t psg_midi_inuse[3 * PSG_NUMBERS];
uint8_t psg_midi_inuse_ch[3 * PSG_NUMBERS];
uint8_t psg_midi_note[3 * PSG_NUMBERS];

uint8_t midi_ch_volume[16];

const uint16_t psg_volume[] = { 0x00, 0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x04,
        0x05, 0x06, 0x07, 0x08, 0x09, 0x0b, 0x0d, 0x10, 0x13, 0x17, 0x1b, 0x20,
        0x26, 0x2d, 0x36, 0x40, 0x4c, 0x5a, 0x6b, 0x80, 0x98, 0xb4, 0xd6, 0xff };

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

void TIM1_PWMOut_Init(u16 arr, u16 psc, u16 ccp) {
    TIM_OCInitTypeDef TIM_OCInitStructure = { 0 };
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure = { 0 };

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

void toneinit(void) {

    GPIO_InitTypeDef GPIO_InitStructure = { 0 };

#ifdef CH32V20x_D6
    RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA | RCC_APB2Periph_TIM1, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
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
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;    // for debug
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init( GPIOC, &GPIO_InitStructure);
#endif

}

void psg_reset(void) {

    for (int i = 0; i < 3 * PSG_NUMBERS; i++) {
        psg_osc_interval[i] = UINT32_MAX;
        psg_osc_counter[i] = 0;
        psg_osc_freq[i] = 0;
        psg_tone_on[i] = 0;
        psg_tone_volume[i] = 0;
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

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1,
            ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

#ifdef PSG_DEBUG
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
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

    /*
     while(USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET);
     return USART_ReceiveData(USART1);
     */
}

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

    // Intialize systick interrupt by sampling frequency

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    // NVIC_SetPriority(SysTicK_IRQn, 0);
    NVIC_EnableIRQ(SysTicK_IRQn);
    SysTick->SR &= ~(1 << 0);
    SysTick->CMP = (SystemCoreClock / SAMPLING_FREQ) - 1;
    SysTick->CNT = 0;
    SysTick->CTLR = 0xF;

    for (int i = 0; i < RX_BUFFER_LEN; i++) {
        rxbuff[i] = 255;
    }

#ifdef PSG_DEBUG
    USART_Printf_Init(115200);
    printf("SystemClk:%d\r\n", SystemCoreClock);
#endif

    usart_init(SERIAL_SPEED);

    DMA_Rx_Init( DMA1_Channel5, (u32) &USART1->DATAR, (u32) &rxbuff,
    RX_BUFFER_LEN);

    toneinit();
    TIM1_PWMOut_Init(256, 0, 255);  // 48MHz * 256 = 5us

    // Initialize PSG

    //   while(1) {
    //      printf("%x ",usart_getch());
    //  }

    psg_reset();

    while(1) {

        // Listen USART

        midicmd=usart_getch();
        midich=midicmd&0xf;

#ifdef PSG_DEBUG

        printf("%02x\n\r",midicmd);

#endif

        switch(midicmd&0xf0) {
            case 0x80: // Note off
            midinote=usart_getch();
            midivel=usart_getch();
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
            midinote=usart_getch();
            midivel=usart_getch();
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
            midicc1=usart_getch();
            switch(midicc1) {
                case 7:
                case 11: // Expression
                midicc2=usart_getch();
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
    uint16_t master_volume;
    uint8_t tone_output[3 * PSG_NUMBERS];
    uint8_t enable_channels;

//    TIM1->CH4CVR = psg_master_volume / (3 * PSG_NUMBERS);

    TIM1->CH4CVR = psg_master_volume;

#ifdef PSG_DEBUG
    GPIO_WriteBit(GPIOC, GPIO_Pin_3, Bit_SET);
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

    master_volume = 0;
    enable_channels = 0;

    for (int i = 0; i < 3 * PSG_NUMBERS; i++) {
        if (psg_tone_on[i] == 1) {
            if (tone_output[i] != 0) {
                master_volume += psg_volume[midi_ch_volume[psg_midi_inuse_ch[i]]
                        * 2 + 1];
            }
            enable_channels++;
        }
    }

//    psg_master_volume = master_volume / (3 * PSG_NUMBERS);

    psg_master_volume = master_volume / PSG_DEVIDE_FACTOR;

//    if(enable_channels!=0) {
//        psg_master_volume = master_volume / enable_channels;
//    } else {
//        psg_master_volume=0;
//    }

    if (psg_master_volume > 255)
        psg_master_volume = 255;
    SysTick->SR &= 0;

#ifdef PSG_DEBUG
    GPIO_WriteBit(GPIOC, GPIO_Pin_3, Bit_RESET);
#endif

}
