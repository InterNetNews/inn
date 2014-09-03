/* $Id$ */
/* hex test suite. */

#include "config.h"
#include "clibrary.h"

#include "inn/utility.h"
#include "tap/basic.h"

int
main(void)
{
    static const unsigned char d1[] = { '\0', '\377', '\377', '\0' };
    static const unsigned char d2[] = { '\1', '\2', '\276', '\277' };
    static const unsigned char d3[] = { '\17', '\160', '\0', '\0' };
    static const char t1[] = "00FFFF00";
    static const char t2[] = "0102BEBF";
    static const char t3[] = "0F7!!!";
    static const char t4[] = "0102bebf";
    unsigned char dout[4];
    char tout[9];

    test_init(16);

    inn_encode_hex(d1, sizeof(d1), tout, sizeof(tout));
    ok_string(1, t1, tout);
    inn_encode_hex(d2, sizeof(d2), tout, sizeof(tout));
    ok_string(2, t2, tout);

    inn_encode_hex(d1, 1, tout, sizeof(tout));
    ok_string(3, "00", tout);
    inn_encode_hex(d2, 0, tout, sizeof(tout));
    ok_string(4, "", tout);
    inn_encode_hex(d2, sizeof(d2), tout, 2);
    ok_string(5, "0", tout);
    inn_encode_hex(d2, sizeof(d2), tout, 3);
    ok_string(6, "01", tout);
    inn_encode_hex(d2, sizeof(d2), tout, 6);
    ok_string(7, "0102B", tout);
    inn_encode_hex(d2, sizeof(d2), tout, 0);
    ok_string(8, "0102B", tout);
    inn_encode_hex(d2, sizeof(d2), tout, 1);
    ok_string(9, "", tout);

    inn_decode_hex(t1, dout, sizeof(dout));
    ok(10, memcmp(d1, dout, sizeof(dout)) == 0);
    inn_decode_hex(t2, dout, sizeof(dout));
    ok(11, memcmp(d2, dout, sizeof(dout)) == 0);
    inn_decode_hex(t3, dout, sizeof(dout));
    ok(12, memcmp(d3, dout, sizeof(dout)) == 0);
    inn_decode_hex(t4, dout, sizeof(dout));
    ok(13, memcmp(d2, dout, sizeof(dout)) == 0);

    inn_decode_hex(t2, dout, sizeof(dout));
    inn_decode_hex(t1, dout, 1);
    ok(14, memcmp("", dout, 1) == 0);
    ok(15, memcmp("\2\276\277", dout + 1, 3) == 0);
    inn_decode_hex(t2, dout, sizeof(dout));
    inn_decode_hex(t1, dout, 0);
    ok(16, memcmp(d2, dout, sizeof(dout)) == 0);

    return 0;
}
