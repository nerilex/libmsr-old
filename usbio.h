
#ifndef _SERIALIO_H_

extern int serial_open (char *, int *,  int, speed_t);
extern int serial_close (int);
extern int serial_readchar (int, uint8_t *);
extern int serial_write (int, void *, size_t);
extern int serial_read (int, void *, size_t);

extern int usb_open (char *, int *,  int, speed_t);
extern int usb_close (int);
extern int usb_readchar (int, uint8_t *);
extern int usb_write (int, void *, size_t);
extern int usb_read (int, void *, size_t);
extern int usb_commit (int);


#endif /* _SERIALIO_H_ */
