void kernel_main()
{
    volatile char* video = (volatile char*) 0xB8000;

    // Clear whole screen
    for(int i = 0; i < 80 * 25; i++)
    {
        video[i * 2] = ' ';
        video[i * 2 + 1] = 0x1F;
    }

    const char* text = "Welcome to turbOS!";

    int row = 12;
    int col = 32;

    for(int i = 0; text[i] != '\0'; i++)
    {
        int index = ((row * 80) + col + i) * 2;

        video[index] = text[i];
        video[index + 1] = 0x0F;
    }

    while(1);
}
