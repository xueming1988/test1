#ifndef __ALEXA_PRODUCTS_MACRO_H__
#define __ALEXA_PRODUCTS_MACRO_H__

#define ALEXA_PRODUCTS_FILE_PATH "/system/workdir/misc/config.bin"

#ifndef ALEXA_BETA
#define ALEXA_BETA (0)
#endif

#ifdef A31_JAM
#define ALEXA_DEFAULT_DEVICE_NAME ("jam_hx_p590")
#define ALEXA_DEFAULT_DEVICE_NAME_BETA ("jam_voice_BETA")
//#define ALEXA_DEFAULT_CLIENT_ID ("amzn1.application-oa2-client.4de786f2ba2c452e8705ac434e25b107")
//#define ALEXA_DEFAULT_CLIENT_SECRET
//("a67411dcb744a4cdcf141e3845841ed37f8608a6d0dd48fc43710d616c054e01")
#define ALEXA_DEFAULT_CLIENT_ID                                                                    \
    ("\x90\xeb\x8c\xd7\xc\x6e\x2c\xbf\x33\x28\xf\x35\xf0\x41\xa2\xd6\x6b\x72\x6b\x50\x2c\xf6\x2e"  \
     "\xd\x7d\x34\xbf\xfa\x2f\x7\xd5\x5c\x3\x28\x4a\x37\x37\x1a\x6f\x74\xc2\xa\x69\xd0\x59\x2f"    \
     "\xe8\x26\x87\xe5\xff\xdb\xa9\x95\xb\xc4\xb7\xce\x67\x1\xa5")
#define ALEXA_DEFAULT_CLIENT_SECRET                                                                \
    ("\x90\xb0\xc1\x8d\xc\x71\x29\xac\x21\x73\x52\x62\xf0\x1\xa8\xdd\x66\x39\x35\x5\x2f\xbe\x7e"   \
     "\x59\x20\x64\xe9\xba\x30\x56\xd5\xa\x3\x76\x44\x67\x35\x40\x6f\x70\xc5\xe\x38\x86\x8\x2f"    \
     "\xb9\x75\x86\xb7\xab\xde\xaa\xc5\x58\xc7\xb4\xcf\x66\x4\xa6\x39\xe8\x15")

#define ALEXA_DEFAULT_SERVER_URL ("")

#elif defined(A31_EDGE)
#define ALEXA_DEFAULT_DEVICE_NAME ("Edge")
#ifndef ALEXA_BETA
#error "doesn't have a beta device id, please regiseter on amazon developer"
#endif
//#define ALEXA_DEFAULT_CLIENT_ID ("amzn1.application-oa2-client.625473a4dd394d07a4594972c28b2d44")
//#define ALEXA_DEFAULT_CLIENT_SECRET
//("0d3bd24e22398783e7e9103024e8b8dc8f997abfe3ebc82005f06d8f135752a3")
#define ALEXA_DEFAULT_CLIENT_ID                                                                    \
    ("\xab\xf3\x7e\x47\x97\x87\xb7\xd6\xab\x44\x8b\xf0\x74\x3d\xaa\xfb\xb6\xe0\x59\x4a\x1c\x6\x82" \
     "\xc7\xc6\xeb\x37\x27\x63\x65\x58\x8c\x3a\x5b\xd6\x2f\x3e\xf9\x2e\x15\xc8\x50\xce\x53\x27"    \
     "\xa0\x6\x29\x3d\xc5\xe9\xc5\x34\x74\x2d\x8a\xf\x56\x35\x2a\x26")
#define ALEXA_DEFAULT_CLIENT_SECRET                                                                \
    ("\xfa\xfa\x37\x4b\xc2\x9b\xe2\xc3\xe9\x1a\xd1\xaa\x2d\x7e\xfb\xa7\xbd\xfa\x53\x12\x1f\x1b"    \
     "\xd2\x9b\x9d\xba\x3c\x6b\x2f\x6b\xe\xda\x36\xa\xdc\x77\x3d\xfc\x28\x40\x94\x57\xcf\x1\x73"   \
     "\xf9\x0\x2c\x34\xc4\xb6\xc2\x30\x73\x27\xd4\x5c\x57\x64\x29\x27\x3e\x1c\xe9")
#define ALEXA_DEFAULT_SERVER_URL ("")

#elif defined(A31_WB_22)
#define ALEXA_DEFAULT_DEVICE_NAME ("WB_22")
#define ALEXA_DEFAULT_DEVICE_NAME_BETA ("WB_22_BETA")
//#define ALEXA_DEFAULT_CLIENT_ID ("amzn1.application-oa2-client.c4a4384711c94197a1ac39b39ff1852c")
//#define ALEXA_DEFAULT_CLIENT_SECRET
//("266eacac8d28b3158042f991822b9887f78f93d77190bb6341d5143ea8205daa")
#define ALEXA_DEFAULT_CLIENT_ID                                                                    \
    ("\xab\xf3\x7e\x47\x97\x87\xb7\xd6\xab\x44\x8b\xf0\x74\x3d\xaa\xfb\xb6\xe0\x59\x4a\x1c\x6\x82" \
     "\xc7\xc6\xeb\x37\x27\x63\x30\x5e\xd8\x3a\x5f\xdd\x7a\x3d\xac\x7b\x45\xc8\x50\x9b\x5a\x27"    \
     "\xa0\x3\x7d\x67\xc2\xe9\x90\x35\x2e\x79\xd4\x5c\x5c\x64\x2c\x71")
#define ALEXA_DEFAULT_CLIENT_SECRET                                                                \
    ("\xf8\xa8\x32\x4c\xc7\xca\xb7\xc5\xe3\x4c\xd0\xab\x77\x7a\xf2\xa1\xe0\xfd\x2\x19\x48\x12\xd8" \
     "\x9a\x97\xbc\x6b\x31\x74\x6b\x52\x8e\x68\x5b\xdd\x28\x33\xae\x2e\x11\xc6\x55\x93\x53\x72"    \
     "\xa3\x4\x2f\x30\xc0\xb4\xc7\x37\x23\x2c\xd7\xc\x5c\x63\x2e\x27\x68\x1c\xbb")
#define ALEXA_DEFAULT_SERVER_URL ("")

#elif defined(FENDA_W55)
#define ALEXA_DEFAULT_DEVICE_NAME ("PIKA")
#define ALEXA_DEFAULT_DEVICE_NAME_BETA ("PIKA_BETA")
//#define ALEXA_DEFAULT_CLIENT_ID ("amzn1.application-oa2-client.bf4797ac034b451a81c2eac24838d5a9")
//#define ALEXA_DEFAULT_CLIENT_SECRET
//("6eafe9f1dcea2e6e2b7da84920b0f0e0220b9a01ede21474da476702e13b0ae0")
#define ALEXA_DEFAULT_CLIENT_ID                                                                    \
    ("\xab\xf3\x7e\x47\x97\x87\xb7\xd6\xab\x44\x8b\xf0\x74\x3d\xaa\xfb\xb6\xe0\x59\x4a\x1c\x6\x82" \
     "\xc7\xc6\xeb\x37\x27\x63\x31\xc\x8d\x39\x55\xd2\x2f\x69\xad\x79\x12\x93\x50\x9f\x52\x71\xf9" \
     "\x3\x7f\x36\x94\xb1\x91\x34\x23\x27\x81\x55\x0\x64\x7f\x2b")
#define ALEXA_DEFAULT_CLIENT_SECRET                                                                \
    ("\xfc\xfb\x65\x4f\xc3\x90\xb0\x97\xbf\x4b\x87\xf2\x27\x2c\xf5\xf1\xea\xaf\x1\x4f\x4f\x13\xd5" \
     "\x92\x9d\xbe\x3b\x63\x2b\x63\xf\x89\x3c\x5e\xd5\x2c\x33\xfc\x7a\x17\x94\x0\xcf\x51\x21\xf5"  \
     "\x5\x28\x60\x90\xe4\xc5\x30\x20\x2f\x80\x8\x55\x62\x7c\x22\x6d\x18\xea")
#define ALEXA_DEFAULT_SERVER_URL ("")

#elif defined(S11_EVB_EUFY_DOT)
#define ALEXA_DEFAULT_DEVICE_NAME ("Eufy_Genie")
#define ALEXA_DEFAULT_DEVICE_NAME_BETA ("Eufy_Genie_BETA")
//#define ALEXA_DEFAULT_CLIENT_ID ("amzn1.application-oa2-client.4999715c375c4f9b8b7a4bf2071efb6e")
//#define ALEXA_DEFAULT_CLIENT_SECRET
//("63736e48ba2f403d203bf9dac7db5bf9ef8a984aa74258d48d81b5f5f69e5021")
#define ALEXA_DEFAULT_CLIENT_ID                                                                    \
    ("\xab\xf3\x7e\x47\x97\x87\xb7\xd6\xab\x44\x8b\xf0\x74\x3d\xaa\xfb\xb6\xe0\x59\x4a\x1c\x6\x82" \
     "\xc7\xc6\xeb\x37\x27\x63\x67\x53\x80\x37\x5b\xd4\x7b\x69\xae\x7d\x13\x92\x50\xcc\x5a\x72"    \
     "\xf9\x50\x2b\x65\xc5\xb2\x94\x34\x27\x28\x83\x8\x2\x33\x28\x77")
#define ALEXA_DEFAULT_CLIENT_SECRET                                                                \
    ("\xfc\xad\x33\x1a\x90\xcc\xe2\x9e\xb9\x49\xd0\xf5\x21\x79\xf0\xf0\xea\xfd\x5\x49\x48\x12\x85" \
     "\xca\xcc\xb9\x3d\x31\x78\x31\xc\x80\x6b\xa\xdd\x2f\x33\xa5\x7e\x47\x90\x53\x9e\x51\x25\xf9"  \
     "\x56\x28\x3c\x95\xe8\xc3\x64\x22\x79\x87\xb\x52\x68\x7b\x27\x3c\x4f\xeb")
#define ALEXA_DEFAULT_SERVER_URL ("")
#define ALEXA_DEFAULT_DTID ("A26TRKU1GG059T") // eufy genie

#elif defined(A31_MS05)
#define ALEXA_DEFAULT_DEVICE_NAME ("MS05")
#ifndef ALEXA_BETA
#error "doesn't have a beta device id, please regiseter on amazon developer"
#endif
//#define ALEXA_DEFAULT_CLIENT_ID ("amzn1.application-oa2-client.18250ad870cd42aca6fbaf4062964983")
//#define ALEXA_DEFAULT_CLIENT_SECRET
//("c0cf39d49442053fbe27e8f699a3184903a4feea113d4b2752becd3393eb85cb")
#define ALEXA_DEFAULT_CLIENT_ID                                                                    \
    ("\xab\xf3\x7e\x47\x97\x87\xb7\xd6\xab\x44\x8b\xf0\x74\x3d\xaa\xfb\xb6\xe0\x59\x4a\x1c\x6\x82" \
     "\xc7\xc6\xeb\x37\x27\x63\x62\x52\x8b\x3b\x5c\x84\x2a\x32\xaa\x7a\x45\x95\x50\x98\x2\x73\xa0" \
     "\x4\x7a\x66\x90\xb6\xc6\x36\x21\x2d\x8b\x5b\x50\x68\x26\x21")
#define ALEXA_DEFAULT_CLIENT_SECRET                                                                \
    ("\xa9\xae\x67\x4f\x95\x90\xb2\x92\xe2\x1c\xd6\xa1\x25\x7c\xf0\xf2\xba\xa8\x4\x1c\x4b\x13\x87" \
     "\x9d\x96\xb7\x38\x60\x7c\x6b\x5e\x80\x3e\x5f\x84\x7a\x6c\xf8\x2f\x47\xc0\x55\x99\x7\x24\xa3" \
     "\x0\x2b\x31\xc3\xb2\x97\x65\x73\x2c\x81\x54\x57\x34\x7c\x2a\x39\x1e\xb8")
#define ALEXA_DEFAULT_SERVER_URL ("")

#elif defined(A31_E5)
#define ALEXA_DEFAULT_DEVICE_NAME ("E5")
#define ALEXA_DEFAULT_DEVICE_NAME_BETA ("E5_BETA")
//#define ALEXA_DEFAULT_CLIENT_ID ("amzn1.application-oa2-client.c9b39071f646403e9112b67f361f49d4")
//#define ALEXA_DEFAULT_CLIENT_SECRET
//("af08699d73cb2af6d0431c9e5d2e1ab1f5698f1b66e787dda9afff43196a89b7")
#define ALEXA_DEFAULT_CLIENT_ID                                                                    \
    ("\xab\xf3\x7e\x47\x97\x87\xb7\xd6\xab\x44\x8b\xf0\x74\x3d\xaa\xfb\xb6\xe0\x59\x4a\x1c\x6\x82" \
     "\xc7\xc6\xeb\x37\x27\x63\x30\x53\xdb\x3d\x55\xd5\x79\x3b\xfb\x7c\x12\xc7\x50\x9a\x50\x75"    \
     "\xf8\x3\x2d\x36\x93\xe6\xc5\x60\x24\x29\x83\xb\x50\x68\x7a\x26")
#define ALEXA_DEFAULT_CLIENT_SECRET                                                                \
    ("\xab\xf8\x34\x11\x90\x90\xef\xc2\xec\x1b\x81\xf1\x27\x28\xa5\xa2\xbc\xfd\x2\x18\x1f\x48\xd8" \
     "\xce\x9a\xea\x6b\x36\x7c\x32\x8\x88\x68\x59\xd3\x77\x32\xfb\x7b\x44\xc7\x52\xcf\x54\x28\xf6" \
     "\x56\x78\x65\xc8\xb1\x94\x60\x71\x2b\x81\x5c\x5d\x67\x7f\x2a\x35\x1f\xed")
#define ALEXA_DEFAULT_SERVER_URL ("")

#elif defined(A31_AQUA)
#define ALEXA_DEFAULT_DEVICE_NAME ("AQUA")
#ifndef ALEXA_BETA
#error "doesn't have a beta device id, please regiseter on amazon developer"
#endif
//#define ALEXA_DEFAULT_CLIENT_ID ("amzn1.application-oa2-client.0b4a7e52e15b4cb0a4922e77bf5f36d6")
//#define ALEXA_DEFAULT_CLIENT_SECRET
//("98ba847bdc9f35521fe3d7a41823d72b33e130bd170634df48d476e0f3e0ef25")
#define ALEXA_DEFAULT_CLIENT_ID                                                                    \
    ("\xab\xf3\x7e\x47\x97\x87\xb7\xd6\xab\x44\x8b\xf0\x74\x3d\xaa\xfb\xb6\xe0\x59\x4a\x1c\x6\x82" \
     "\xc7\xc6\xeb\x37\x27\x63\x63\x8\x8d\x6f\x5b\x80\x7b\x38\xf8\x7b\x13\x93\x50\xc9\x1\x20\xa0"  \
     "\x6\x25\x36\xc3\xb5\xc5\x31\x75\x79\x87\xb\x57\x67\x7a\x24")
#define ALEXA_DEFAULT_CLIENT_SECRET                                                                \
    ("\xf3\xa6\x66\x48\x9e\x9d\xe1\xc4\xbf\x4b\xdb\xf5\x26\x7c\xf6\xa6\xe9\xab\x53\x18\x4a\x1c"    \
     "\x80\x9f\x9e\xb6\x6b\x60\x29\x64\x58\xdb\x3d\x5f\x80\x7f\x39\xad\x28\x42\xc0\x53\x9a\x55"    \
     "\x23\xf5\x56\x7a\x30\xc9\xb4\xc6\x31\x21\x7a\x82\xb\x57\x34\x2e\x77\x6a\x4f\xef")
#define ALEXA_DEFAULT_SERVER_URL ("")

#elif defined(A31_MARK)
#define ALEXA_DEFAULT_DEVICE_NAME ("Mark")
#ifndef ALEXA_BETA
#error "doesn't have a beta device id, please regiseter on amazon developer"
#endif
//#define ALEXA_DEFAULT_CLIENT_ID ("amzn1.application-oa2-client.48ff12ce607a466c8cdc7dcce3cf8a82")
//#define ALEXA_DEFAULT_CLIENT_SECRET
//("3eb2f6e88ca9ea44bfa99604e972fc15c7381438e05b4cb95ecfb57c8972b4c6")
#define ALEXA_DEFAULT_CLIENT_ID                                                                    \
    ("\xab\xf3\x7e\x47\x97\x87\xb7\xd6\xab\x44\x8b\xf0\x74\x3d\xaa\xfb\xb6\xe0\x59\x4a\x1c\x6\x82" \
     "\xc7\xc6\xeb\x37\x27\x63\x67\x52\xdf\x68\x5d\xd7\x2d\x6f\xab\x7a\x11\x90\x50\x9c\x55\x73"    \
     "\xf9\x51\x78\x67\xc6\xb4\x91\x65\x72\x2c\xd1\xb\x5c\x30\x26\x20")
#define ALEXA_DEFAULT_CLIENT_SECRET                                                                \
    ("\xf9\xfb\x66\x1b\xc0\x9f\xb3\x9e\xe3\x4b\x83\xaa\x70\x28\xf7\xa0\xba\xab\x57\x12\x17\x1d"    \
     "\xd1\x9f\xca\xb7\x6e\x61\x2b\x30\x5b\x8c\x6d\x5b\xd6\x76\x3b\xa9\x79\x1e\x94\x54\x9f\x1\x24" \
     "\xa2\x50\x25\x31\x94\xb3\x94\x64\x22\x28\xd1\x55\x5d\x66\x2c\x70\x38\x1e\xec")
#define ALEXA_DEFAULT_SERVER_URL ("")

#elif defined(A31_ES001)
#define ALEXA_DEFAULT_DEVICE_NAME ("ES001")
#ifndef ALEXA_BETA
#error "doesn't have a beta device id, please regiseter on amazon developer"
#endif
//#define ALEXA_DEFAULT_CLIENT_ID ("amzn1.application-oa2-client.561e1960ac2c42fab66e39ca1fd8a99a")
//#define ALEXA_DEFAULT_CLIENT_SECRET
//("fc320ad84367d992594f58d48dcccf2a827189674f8d8d73552e4f2a65b86b51")
#define ALEXA_DEFAULT_CLIENT_ID                                                                    \
    ("\xab\xf3\x7e\x47\x97\x87\xb7\xd6\xab\x44\x8b\xf0\x74\x3d\xaa\xfb\xb6\xe0\x59\x4a\x1c\x6\x82" \
     "\xc7\xc6\xeb\x37\x27\x63\x66\x5c\x88\x6b\x5d\xdc\x78\x3a\xfc\x29\x14\x92\x50\x98\x5\x71\xa3" \
     "\x4\x2a\x61\xc2\xe9\x91\x67\x26\x79\xd6\x55\x5\x68\x27\x73")
#define ALEXA_DEFAULT_CLIENT_SECRET                                                                \
    ("\xac\xfd\x37\x1b\x96\xc8\xb2\x9e\xef\x1b\xd4\xa4\x71\x70\xfa\xa6\xed\xf4\x2\x4d\x1b\x13\x85" \
     "\x9f\x97\xea\x3a\x30\x2e\x35\x58\xd8\x36\x5e\xd2\x7f\x32\xa4\x7c\x11\xc5\x2\x92\x7\x28\xa5"  \
     "\x5\x2f\x31\xc4\xe2\x97\x32\x71\x2d\xd3\x5b\x51\x33\x26\x24\x6e\x48\xeb")
#define ALEXA_DEFAULT_SERVER_URL ("")

#elif defined(A31_Flyears)
//#define ALEXA_DEFAULT_CLIENT_ID ("amzn1.application-oa2-client.5aab5a0f4dc94a73bb5c6205939fd250")
//#define ALEXA_DEFAULT_CLIENT_SECRET
//("6215faab26b38dbe74bfff7847701c4f0a2e9de7e4d88ae6a9254bff57c3a0d5")
#define ALEXA_DEFAULT_CLIENT_ID                                                                    \
    ("\xab\xf3\x7e\x47\x97\x87\xb7\xd6\xab\x44\x8b\xf0\x74\x3d\xaa\xfb\xb6\xe0\x59\x4a\x1c\x6\x82" \
     "\xc7\xc6\xeb\x37\x27\x63\x66\xb\xd8\x6c\x59\x84\x7e\x6c\xa9\x2e\x45\xc8\x50\xcb\x54\x23\xa3" \
     "\x50\x29\x67\xc7\xe2\xc2\x33\x2e\x2c\x8b\xb\x0\x63\x2b\x22")
#define ALEXA_DEFAULT_CLIENT_SECRET                                                                \
    ("\xfc\xac\x35\x1c\xc0\xc8\xb7\xc4\xe9\x1e\x80\xa0\x2d\x2d\xa1\xf1\xef\xf9\x54\x4d\x48\x4d"    \
     "\xd6\x93\x9b\xb9\x6e\x63\x7c\x30\x5e\xdf\x3e\xd\xd7\x2b\x33\xf9\x2f\x11\x94\x50\xce\x5b\x28" \
     "\xa0\x57\x2a\x65\xc8\xe2\xc7\x32\x75\x79\xd4\x58\x53\x32\x2d\x73\x3c\x19\xef")
#define ALEXA_DEFAULT_SERVER_URL ("")

#else

#define ALEXA_DEFAULT_DEVICE_NAME ("linkplay_alexa_device_BETA") // always use beta ID of linkplay
#define ALEXA_DEFAULT_DEVICE_NAME_BETA ("linkplay_alexa_device_BETA")
//#define ALEXA_DEFAULT_CLIENT_ID ("amzn1.application-oa2-client.4ed2bb5c31d24af58613bf792ecc37d5")
//#define ALEXA_DEFAULT_CLIENT_SECRET
//("f67194971bc9d972cb2384d2f1b378c4c1b67f8784f237f3cde1e4a104600c54")
#define ALEXA_DEFAULT_CLIENT_ID                                                                    \
    ("\xab\xf3\x7e\x47\x97\x87\xb7\xd6\xab\x44\x8b\xf0\x74\x3d\xaa\xfb\xb6\xe0\x59\x4a\x1c\x6\x82" \
     "\xc7\xc6\xeb\x37\x27\x63\x67\xf\xdd\x3c\xe\x87\x7b\x69\xae\x7b\x42\xc3\x50\xcb\x5\x25\xf9"   \
     "\x4\x2d\x37\x93\xb6\xc5\x3f\x25\x7a\xd1\xe\x57\x66\x7a\x27")
#define ALEXA_DEFAULT_CLIENT_SECRET                                                                \
    ("\xac\xa8\x33\x18\x9f\x9d\xef\x91\xea\x4a\x81\xaa\x71\x70\xf4\xa6\xbb\xaf\x4\x18\x16\x1f\x85" \
     "\x99\xc9\xbf\x3b\x60\x7a\x6b\x9\x8d\x6d\x5d\x87\x78\x3d\xfb\x72\x11\xc9\x50\xcc\x51\x23\xf6" \
     "\x54\x2f\x67\x95\xb5\xc3\x63\x23\x7e\x83\x5d\x50\x67\x2e\x22\x6f\x48\xee")
#define ALEXA_DEFAULT_SERVER_URL ("")
#endif

#endif
