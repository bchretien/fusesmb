#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "smbctx.h"

config_t *fusesmb_auth_fn_cfg = NULL;
pthread_mutex_t *fusesmb_auth_fn_cfg_mutex = NULL;

static int nmblookup(const char *ip_server, char *output, size_t outputsize)
{
    char ipcmd[1024] = "nmblookup -A ";
    strcat(ipcmd, ip_server);
    FILE *pipe = popen(ipcmd, "r");
    if (NULL == pipe)
        return -1;
    while (!feof(pipe))
    {
        char buf2[4096];
        char buf[4096];
        char ip[32];

        char *start = buf;
        if (NULL == fgets(buf2, 4096, pipe))
            continue;
        if (strncmp(buf2, "Looking up status of ", strlen("Looking up status of ")) == 0)
        {
            char *tmp = rindex(buf2, ' ');
            tmp++;
            char *end = index(tmp, '\n');
            *end = '\0';
            strcpy(ip, tmp);
            printf("%s\n", ip);
        }
        else
        {
            continue;
        }

        while (!feof(pipe))
        {

            if (NULL == fgets(buf, 4096, pipe))
                break;
            char *sep = buf;

            if (*buf != '\t')
                break;
            if (NULL != strstr(buf, "<GROUP>"))
                break;
            if (NULL == (sep = strstr(buf, "<00>")))
                break;
            *sep = '\0';

            start++;

            while (*sep == '\t' || *sep == ' ' || *sep == '\0')
            {
                *sep = '\0';
                sep--;
            }
            strncpy(output, start, outputsize);
            printf("nmblookupje: %s\n", start);
        }

    }
    pclose(pipe);
    return 0;
}


static void fusesmb_auth_fn(const char *server, const char *share,
                            char *workgroup, int wgmaxlen,
                            char *username, int unmaxlen,
                            char *password, int pwmaxlen)
{
    //(void)server;
    //(void)share;
    (void)workgroup;
    (void)wgmaxlen;

    /* No nmblookup necessary because not using ip's here */
    char sv_section[1024] = "/";
    strcat(sv_section, server);
    char *un, *pw;
    pthread_mutex_lock(fusesmb_auth_fn_cfg_mutex);
    if (0 == config_read_string(fusesmb_auth_fn_cfg, sv_section, "username", &un))
    {
        if (0 == config_read_string(fusesmb_auth_fn_cfg, sv_section, "password", &pw))
        {
            pthread_mutex_unlock(fusesmb_auth_fn_cfg_mutex);
            printf("%s, %s, %s\n", sv_section, un, pw);
            strncpy(username, un, unmaxlen);
            strncpy(password, pw, pwmaxlen);
            free(un);
            free(pw);
            return;
        }
        free(un);
    }

    strcat(sv_section, "/");
    strcat(sv_section, share);

    if (0 == config_read_string(fusesmb_auth_fn_cfg, sv_section, "username", &un))
    {
        if (0 == config_read_string(fusesmb_auth_fn_cfg, sv_section, "password", &pw))
        {
            pthread_mutex_unlock(fusesmb_auth_fn_cfg_mutex);
            printf("%s, %s, %s\n", sv_section, un, pw);
            strncpy(username, un, unmaxlen);
            strncpy(password, pw, pwmaxlen);
            free(un);
            free(pw);
            return;
        }
        free(un);
    }
    if (0 == config_read_string(fusesmb_auth_fn_cfg, "global", "username", &un))
    {
        if (0 == config_read_string(fusesmb_auth_fn_cfg, "global", "password", &pw))
        {
            pthread_mutex_unlock(fusesmb_auth_fn_cfg_mutex);
            printf("%s, %s, %s\n", sv_section, un, pw);
            strncpy(username, un, unmaxlen);
            strncpy(password, pw, pwmaxlen);
            free(un);
            free(pw);
            return;
        }
        free(un);
    }
    pthread_mutex_unlock(fusesmb_auth_fn_cfg_mutex);
    username = NULL;
    password = NULL;
    return;
}

static void fusesmb_cache_auth_fn(const char *server, const char *share,
                                  char *workgroup, int wgmaxlen,
                                  char *username, int unmaxlen,
                                  char *password, int pwmaxlen)
{
    (void)workgroup;
    (void)wgmaxlen;
    char sv[1024];
    nmblookup(server, sv, 1024);
    char sv_section[1024] = "/";
    strcat(sv_section, sv);
    char *un, *pw;
    if (0 == config_read_string(fusesmb_auth_fn_cfg, sv_section, "username", &un))
    {
        if (0 == config_read_string(fusesmb_auth_fn_cfg, sv_section, "password", &pw))
        {
            printf("%s, %s, %s\n", sv_section, un, pw);
            strncpy(username, un, unmaxlen);
            strncpy(password, pw, pwmaxlen);
            free(un);
            free(pw);
            return;
        }
        free(un);
    }
    strcat(sv_section, "/");
    strcat(sv_section, share);
    if (0 == config_read_string(fusesmb_auth_fn_cfg, sv_section, "username", &un))
    {
        if (0 == config_read_string(fusesmb_auth_fn_cfg, sv_section, "password", &pw))
        {
            printf("%s, %s, %s\n", sv_section, un, pw);
            strncpy(username, un, unmaxlen);
            strncpy(password, pw, pwmaxlen);
            free(un);
            free(pw);
            return;
        }
        free(un);
    }
    if (0 == config_read_string(fusesmb_auth_fn_cfg, "global", "username", &un))
    {
        if (0 == config_read_string(fusesmb_auth_fn_cfg, "global", "password", &pw))
        {
            printf("%s, %s, %s\n", sv_section, un, pw);
            strncpy(username, un, unmaxlen);
            strncpy(password, pw, pwmaxlen);
            free(un);
            free(pw);
            return;
        }
        free(un);
    }
    username = NULL;
    password = NULL;
    return;
}

/*
 * Create a new libsmbclient context with all necessary options
 */
static SMBCCTX *fusesmb_context(smbc_get_auth_data_fn fn)
{
    /* Initializing libsbmclient */
    SMBCCTX *ctx;
    ctx = smbc_new_context();
    if (ctx == NULL)
        return NULL;

    ctx->callbacks.auth_fn = fn;
    //ctx->debug = 4;
    /* Timeout a bit bigger, by Jim Ramsay */
    ctx->timeout = 10000;       //10 seconds
    /* Kerberos authentication by Esben Nielsen */
#if defined(SMB_CTX_FLAG_USE_KERBEROS) && defined(SMB_CTX_FLAG_FALLBACK_AFTER_KERBEROS)
    ctx->flags |=
        SMB_CTX_FLAG_USE_KERBEROS | SMB_CTX_FLAG_FALLBACK_AFTER_KERBEROS;
#endif
    //ctx->options.one_share_per_server = 1;
    ctx = smbc_init_context(ctx);
    return ctx;
}

SMBCCTX *fusesmb_cache_new_context(config_t *cf)
{
    fusesmb_auth_fn_cfg = cf;
    return fusesmb_context(fusesmb_cache_auth_fn);
}

SMBCCTX *fusesmb_new_context(config_t *cf, pthread_mutex_t *mutex)
{
    fusesmb_auth_fn_cfg = cf;
    fusesmb_auth_fn_cfg_mutex = mutex;
    return fusesmb_context(fusesmb_auth_fn);
}

