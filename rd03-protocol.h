#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// parsing state
typedef enum {
    HEADER,
    LEN_1,
    LEN_2,
    DATA,
    FOOTER,
} state_t;

class RD03Protocol {

  private:
    state_t _state;
    uint8_t _buf[64];
    uint8_t _len;
    uint8_t _idx;
    uint32_t _delim;
    uint32_t _header;
    uint32_t _footer;

  public:
    // FAFBFCFD/01020304 for ACK
    // F1F2F3F4/F5F6F7F8 for report
     RD03Protocol(uint32_t header, uint32_t footer);

    // builds a command
    size_t build_command(uint8_t * buf, uint16_t cmd, size_t cmd_data_len,
                         const uint8_t * cmd_data);

    // processes received data, returns true if measurement data was found
    bool process_rx(uint8_t c);
    void reset_rx(void);

    // call this when process_rx returns true, copies received data into buffer, returns length
    size_t get_data(uint8_t * data);

};
