/* Local-only integration client for the OpenSSL TLS protocol regression test. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libavformat/avformat.h"
#include "libavutil/error.h"
#include "libavutil/opt.h"
#include <openssl/ssl.h>

#ifdef TLS_FAULT_INJECTION
#include <openssl/x509v3.h>
static int fail(const char *name)
{
    const char *value = getenv("TLS_TEST_FAIL");
    return value && !strcmp(value, name);
}
int __real_X509_VERIFY_PARAM_set1_host(X509_VERIFY_PARAM *, const char *, size_t);
int __wrap_X509_VERIFY_PARAM_set1_host(X509_VERIFY_PARAM *p, const char *s, size_t n)
{
    return fail("host") ? 0 : __real_X509_VERIFY_PARAM_set1_host(p, s, n);
}
int __real_X509_VERIFY_PARAM_set1_ip(X509_VERIFY_PARAM *, const unsigned char *, size_t);
int __wrap_X509_VERIFY_PARAM_set1_ip(X509_VERIFY_PARAM *p, const unsigned char *s, size_t n)
{
    return fail("ip") ? 0 : __real_X509_VERIFY_PARAM_set1_ip(p, s, n);
}
int __real_SSL_CTX_set_default_verify_paths(SSL_CTX *);
int __wrap_SSL_CTX_set_default_verify_paths(SSL_CTX *ctx)
{
    return fail("default-ca") ? 0 : __real_SSL_CTX_set_default_verify_paths(ctx);
}
BIO_METHOD *__real_BIO_meth_new(int, const char *);
BIO_METHOD *__wrap_BIO_meth_new(int n, const char *s)
{
    return fail("bio-method") ? NULL : __real_BIO_meth_new(n, s);
}
BIO *__real_BIO_new(const BIO_METHOD *);
BIO *__wrap_BIO_new(const BIO_METHOD *m)
{
    return fail("bio") ? NULL : __real_BIO_new(m);
}
long __real_SSL_ctrl(SSL *, int, long, void *);
long __wrap_SSL_ctrl(SSL *ssl, int cmd, long arg, void *ptr)
{
    if (fail("sni") && cmd == SSL_CTRL_SET_TLSEXT_HOSTNAME)
        return 0;
    return __real_SSL_ctrl(ssl, cmd, arg, ptr);
}
#endif

int main(int argc, char **argv)
{
    AVDictionary *options = NULL;
    AVIOContext *io = NULL;
    unsigned char bytes[32] = { 0 };
    int ret;
    if (argc != 5 && argc != 7)
        return 2;
    printf("TLS_OPENSSL_VERSION: %s\n", OpenSSL_version(OPENSSL_VERSION));
    avformat_network_init();
    if (strcmp(argv[2], "default"))
        av_dict_set(&options, "tls_verify", argv[2], 0);
    if (strcmp(argv[3], "none"))
        av_dict_set(&options, "ca_file", argv[3], 0);
    if (strcmp(argv[4], "none"))
        av_dict_set(&options, "verifyhost", argv[4], 0);
    if (argc == 7) {
        av_dict_set(&options, "cert_file", argv[5], 0);
        av_dict_set(&options, "key_file", argv[6], 0);
    }
    av_dict_set(&options, "rw_timeout", "2000000", 0);
    ret = avio_open2(&io, argv[1], AVIO_FLAG_READ, NULL, &options);
    av_dict_free(&options);
    if (ret >= 0) {
        ret = avio_read(io, bytes, sizeof(bytes));
        if (ret != 6 || memcmp(bytes, "tls-ok", 6))
            ret = AVERROR_INVALIDDATA;
    }
    if (io)
        avio_closep(&io);
    avformat_network_deinit();
    if (ret < 0) {
        fprintf(stderr, "TLS_RESULT_FAIL: %s (%d)\n", av_err2str(ret), ret);
        return 1;
    }
    puts("TLS_RESULT_OK");
    return 0;
}
