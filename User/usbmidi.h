#define MIDI_RX_PACKETS 64

extern __attribute__ ((aligned(4))) uint8_t  midi_buff[];
extern volatile uint8_t midi_packetlen[];
extern volatile uint32_t midi_load_count;
extern volatile uint32_t midi_remain_count;
extern volatile uint8_t midi_stop_flag;
