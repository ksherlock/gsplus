#ifndef debug_shell_h
#define debug_shell_h

#include <stdint.h>

extern int g_disasm_psr;


struct handle_info {
	uint32_t handle;
	uint32_t address;
	uint32_t size;
	uint16_t owner;
	uint16_t attr;
};

extern char *x_readline(const char *prompt);


/* debug memory access.  does not invoke the i/o machinery */
const uint8_t *debug_read_page(word32 address);

uint8_t debug_read_8(uint32_t address);
uint16_t debug_read_16(uint32_t address);
uint32_t debug_read_24(uint32_t address);
uint32_t debug_read_32(uint32_t address);


/* "goodies" module.  implemented in debug_template.re2c */
int debug_goodies_err(unsigned number);
int debug_goodies_rtype(unsigned number);
int debug_goodies_p8(unsigned number);
int debug_goodies_os(unsigned number);
int debug_goodies_00(unsigned number);
int debug_goodies_01(unsigned number);
int debug_goodies_e0(unsigned number);
int debug_goodies_e1(unsigned number);


extern void debug_load_templates(const char *path);
extern uint32_t debug_apply_template(uint32_t address, const char *name);
extern void debug_load_nifty(const char *path);
extern const char *debug_tool_name(unsigned number, unsigned vector, unsigned full);
extern int lookup_tool_number(const char *name, unsigned vector);

extern int debug_find(const char *needle);


extern uint32_t mini_asm_shell(uint32_t addr);
extern uint32_t sweet16_asm_shell(uint32_t addr);

extern uint32_t do_list(uint32_t address, int lines);


struct handle_info *lookup_handle(uint32_t handle);
struct handle_info *lookup_address(uint32_t address);



#endif
