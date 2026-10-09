#ifndef SPEAKER_H
#define SPEAKER_H

void speaker_start(unsigned int frequency);
void speaker_stop(void);
void speaker_play(unsigned int frequency, unsigned int milliseconds);

#endif
