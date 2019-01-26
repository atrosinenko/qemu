#if 0
    static char data[4 << 20] = "[ ";
    static int countdown = 1010;
    if (countdown-- < 0) {
        for (int i = 0; i < sz; ++i) {
            sprintf(data + 2 + 3 * i, "%02x ", buf[i]);
        }
        data[2 + 3 * sz] = ']';
        data[2 + 3 * sz + 1] = '\n';
        fprintf(stderr, "%s", data);
        fflush(stderr);
        EM_ASM({alert();});

    int fd = open("bin.wasm", O_RDONLY);
    assert(fd);
    sz = read(fd, buf, sizeof (buf));
    EM_ASM({alert($0)}, sz);
    }
#endif
