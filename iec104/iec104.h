#pragma once
#include "iec104_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Information object */
struct iec_object {
        unsigned short		ioa;	/* information object address */
        unsigned char		ioa2;	/* information object address */
	union {
        struct iec_type1	type1;
		struct iec_type7	type7;
		struct iec_type9	type9;
		struct iec_type11	type11;
		struct iec_type13	type13;
		struct iec_type30	type30;
		struct iec_type33	type33;
		struct iec_type34	type34;
		struct iec_type35	type35;
		struct iec_type36	type36;
		struct iec_type37	type37;
		struct iec_type100	type100;
		struct iec_type101	type101;
		struct iec_type103	type103;
	} o;	
}__attribute__((__packed__));


#ifdef __cplusplus
}
