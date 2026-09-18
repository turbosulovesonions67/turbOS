#include "keyboard.h"

#define QUEUE_SIZE 128

static volatile int queue[QUEUE_SIZE];
static volatile unsigned int head = 0;
static volatile unsigned int tail = 0;

static int shift = 0;
static int ctrl = 0;

static const char normal[128] = {
    0,27,'1','2','3','4','5','6','7','8','9','0','-','=',
    KEY_BACKSPACE,KEY_TAB,
    'q','w','e','r','t','y','u','i','o','p','[',']',
    KEY_ENTER,0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',
    0,'*',0,' ',0
};

static const char shifted[128] = {
    0,27,'!','@','#','$','%','^','&','*','(',')','_','+',
    KEY_BACKSPACE,KEY_TAB,
    'Q','W','E','R','T','Y','U','I','O','P','{','}',
    KEY_ENTER,0,
    'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',
    0,'*',0,' ',0
};

static void push_key(int key)
{
    unsigned int next = (head + 1) % QUEUE_SIZE;

    if(next == tail)
        return;

    queue[head] = key;
    head = next;
}

void keyboard_init(void)
{
    head = 0;
    tail = 0;
    shift = 0;
    ctrl = 0;
}

void keyboard_irq(void)
{
    unsigned char sc;

    __asm__ volatile("inb %1,%0" : "=a"(sc) : "Nd"((unsigned short)0x60));

    if(sc & 0x80)
    {
        unsigned char released = sc & 0x7F;

        if(released == 0x2A || released == 0x36)
            shift = 0;

        if(released == 0x1D)
            ctrl = 0;

        return;
    }

    if(sc == 0x2A || sc == 0x36)
    {
        shift = 1;
        return;
    }

    if(sc == 0x1D)
    {
        ctrl = 1;
        push_key(KEY_CTRL);
        return;
    }

    if(sc == 0x48)
    {
        push_key(KEY_UP);
        return;
    }

    if(sc == 0x50)
    {
        push_key(KEY_DOWN);
        return;
    }

    if(sc == 0x4B)
    {
        push_key(KEY_LEFT);
        return;
    }

    if(sc == 0x4D)
    {
        push_key(KEY_RIGHT);
        return;
    }

    if(sc < 128)
    {
        int key = shift ? shifted[sc] : normal[sc];

        if(key)
            push_key(key);
    }
}

int keyboard_getkey(void)
{
    int key;

    if(head == tail)
        return KEY_NONE;

    key = queue[tail];
    tail = (tail + 1) % QUEUE_SIZE;

    return key;
}
