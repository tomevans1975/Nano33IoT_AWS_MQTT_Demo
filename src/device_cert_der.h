// This file needs to be generated from your AWS IoT device certificate.
// Example conversion command:

// openssl x509 -in device.pem.crt -outform DER -out device.der
// xxd -i device.der > device_cert_der.h

#ifndef DEVICE_CERT_DER_H
#define DEVICE_CERT_DER_H

const unsigned char device_cert_der[] = {
  0x30, 0x82, 0x03, 0x2f, 0x30, 0x82, 0x02, 0x17, // ...
};
unsigned int device_cert_der_len = sizeof(device_cert_der);

#endif
