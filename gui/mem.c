

typedef unsigned int mem_size_t;

void *memcpy(void *dst, const void *src, mem_size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    while(n--)
        *d++ = *s++;

    return dst;
}

void *memmove(void *dst, const void *src, mem_size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;

    if(d < s)
    {
        while(n--)
            *d++ = *s++;
    }
    else if(d > s)
    {
        d += n;
        s += n;

        while(n--)
            *--d = *--s;
    }

    return dst;
}

void *memset(void *dst, int value, mem_size_t n)
{
    unsigned char *d = (unsigned char *)dst;

    while(n--)
        *d++ = (unsigned char)value;

    return dst;
}

int memcmp(const void *a, const void *b, mem_size_t n)
{
    const unsigned char *x = (const unsigned char *)a;
    const unsigned char *y = (const unsigned char *)b;

    while(n--)
    {
        if(*x != *y)
            return (int)*x - (int)*y;

        x++;
        y++;
    }

    return 0;
}
