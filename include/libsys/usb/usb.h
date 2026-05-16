#ifndef __USB_H__
#define __USB_H__

#include <stdint.h>
#include <stddef.h>

// USB Versions
#define USB_VERSION_1_0     0x0100
#define USB_VERSION_1_1     0x0110
#define USB_VERSION_2_0     0x0200
#define USB_VERSION_3_0     0x0300
#define USB_VERSION_3_1     0x0310
#define USB_VERSION_3_2     0x0320

// USB Speeds
#define USB_SPEED_UNKNOWN   0
#define USB_SPEED_LOW       1
#define USB_SPEED_FULL      2
#define USB_SPEED_HIGH      3
#define USB_SPEED_SUPER     4
#define USB_SPEED_SUPER_PLUS 5

// USB Device Classes
#define USB_CLASS_PER_INTERFACE   0x00
#define USB_CLASS_AUDIO           0x01
#define USB_CLASS_COMM            0x02
#define USB_CLASS_HID             0x03
#define USB_CLASS_PHYSICAL        0x05
#define USB_CLASS_STILL_IMAGE     0x06
#define USB_CLASS_PRINTER         0x07
#define USB_CLASS_MASS_STORAGE    0x08
#define USB_CLASS_HUB             0x09
#define USB_CLASS_CDC_DATA        0x0a
#define USB_CLASS_CSCID           0x0b
#define USB_CLASS_CONTENT_SEC     0x0d
#define USB_CLASS_VIDEO           0x0e
#define USB_CLASS_PERSONAL_HEALTH 0x0f
#define USB_CLASS_AUDIO_VIDEO     0x10
#define USB_CLASS_BILLBOARD       0x11
#define USB_CLASS_USB_TYPE_C      0x12
#define USB_CLASS_DIAGNOSTIC      0xdc
#define USB_CLASS_WIRELESS        0xe0
#define USB_CLASS_MISC            0xef
#define USB_CLASS_APP_SPEC        0xfe
#define USB_CLASS_VENDOR_SPEC     0xff

// USB Descriptor Types
#define USB_DT_DEVICE                    0x01
#define USB_DT_CONFIG                    0x02
#define USB_DT_STRING                    0x03
#define USB_DT_INTERFACE                 0x04
#define USB_DT_ENDPOINT                  0x05
#define USB_DT_DEVICE_QUALIFIER          0x06
#define USB_DT_OTHER_SPEED_CONFIG        0x07
#define USB_DT_INTERFACE_POWER           0x08
#define USB_DT_OTG                       0x09
#define USB_DT_DEBUG                     0x0a
#define USB_DT_INTERFACE_ASSOC           0x0b
#define USB_DT_SECURITY                  0x0c
#define USB_DT_KEY                       0x0d
#define USB_DT_ENCRYPTION_TYPE           0x0e
#define USB_DT_BOS                       0x0f
#define USB_DT_DEVICE_CAPABILITY         0x10
#define USB_DT_WIRELESS_ENDPOINT_COMP    0x11
#define USB_DT_WIRE_ADAPTER              0x21
#define USB_DT_RPIPE                     0x22
#define USB_DT_CS_DEVICE                 0x23
#define USB_DT_CS_CONFIG                 0x24
#define USB_DT_CS_STRING                 0x25
#define USB_DT_CS_INTERFACE              0x26
#define USB_DT_CS_ENDPOINT               0x27

// USB Request Types
#define USB_TYPE_MASK                    (0x03 << 5)
#define USB_TYPE_STANDARD                (0x00 << 5)
#define USB_TYPE_CLASS                   (0x01 << 5)
#define USB_TYPE_VENDOR                  (0x02 << 5)
#define USB_TYPE_RESERVED                (0x03 << 5)

#define USB_RECIP_MASK                   0x1f
#define USB_RECIP_DEVICE                 0x00
#define USB_RECIP_INTERFACE              0x01
#define USB_RECIP_ENDPOINT               0x02
#define USB_RECIP_OTHER                  0x03
#define USB_RECIP_PORT                   0x04
#define USB_RECIP_RPIPE                  0x05

// USB Standard Requests
#define USB_REQ_GET_STATUS               0x00
#define USB_REQ_CLEAR_FEATURE            0x01
#define USB_REQ_SET_FEATURE              0x03
#define USB_REQ_SET_ADDRESS              0x05
#define USB_REQ_GET_DESCRIPTOR           0x06
#define USB_REQ_SET_DESCRIPTOR           0x07
#define USB_REQ_GET_CONFIGURATION        0x08
#define USB_REQ_SET_CONFIGURATION        0x09
#define USB_REQ_GET_INTERFACE            0x0a
#define USB_REQ_SET_INTERFACE            0x0b
#define USB_REQ_SYNCH_FRAME              0x0c
#define USB_REQ_SET_ENCRYPTION           0x0d
#define USB_REQ_GET_ENCRYPTION           0x0e
#define USB_REQ_RPIPE_ABORT              0x0e
#define USB_REQ_SET_HANDSHAKE            0x0f
#define USB_REQ_RPIPE_RESET              0x0f
#define USB_REQ_GET_HANDSHAKE            0x10
#define USB_REQ_SET_CONNECTION           0x11
#define USB_REQ_SET_SECURITY_DATA        0x12
#define USB_REQ_GET_SECURITY_DATA        0x13
#define USB_REQ_SET_WUSB_DATA            0x14
#define USB_REQ_LOOPBACK_DATA_WRITE      0x15
#define USB_REQ_LOOPBACK_DATA_READ       0x16
#define USB_REQ_SET_INTERFACE_DS         0x17

// USB Endpoint Types
#define USB_ENDPOINT_XFER_CONTROL        0
#define USB_ENDPOINT_XFER_ISOC           1
#define USB_ENDPOINT_XFER_BULK           2
#define USB_ENDPOINT_XFER_INT            3

// USB Endpoint Directions
#define USB_DIR_OUT                      0
#define USB_DIR_IN                       0x80
#define USB_DIR_MASK                     0x80

// USB Status Codes
#define USB_STATUS_SUCCESS               0
#define USB_STATUS_PENDING               1
#define USB_STATUS_ERROR                 -1
#define USB_STATUS_TIMEOUT               -2
#define USB_STATUS_INVALID               -3
#define USB_STATUS_CANCELLED             -4
#define USB_STATUS_NO_MEMORY             -5
#define USB_STATUS_NOT_SUPPORTED         -6

// USB Control Request Structure
typedef struct usb_control_request {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __attribute__((packed)) usb_control_request_t;

// USB Device Descriptor
typedef struct usb_device_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} __attribute__((packed)) usb_device_descriptor_t;

// USB Configuration Descriptor
typedef struct usb_config_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
} __attribute__((packed)) usb_config_descriptor_t;

// USB Interface Descriptor
typedef struct usb_interface_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} __attribute__((packed)) usb_interface_descriptor_t;

// USB Endpoint Descriptor
typedef struct usb_endpoint_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} __attribute__((packed)) usb_endpoint_descriptor_t;

// USB String Descriptor
typedef struct usb_string_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wData[1];  // Variable length
} __attribute__((packed)) usb_string_descriptor_t;

// USB Setup Packet
typedef struct usb_setup_packet {
    union {
        struct {
            uint8_t bmRequestType;
            uint8_t bRequest;
            uint16_t wValue;
            uint16_t wIndex;
            uint16_t wLength;
        };
        uint32_t data[2];
    };
} __attribute__((packed)) usb_setup_packet_t;

// Helper macros
#define USB_ENDPOINT_NUMBER(ep)          ((ep) & 0x0f)
#define USB_ENDPOINT_DIR(ep)             ((ep) & USB_DIR_MASK)
#define USB_ENDPOINT_XFERTYPE(ep)        (((ep) & 0x03))
#define USB_ENDPOINT_MAXP(ep)            ((ep) & 0x07ff)
#define USB_ENDPOINT_INTERVAL(ep)        (((ep) >> 8) & 0xff)

#define USB_MAKE_REQUEST_TYPE(dir, type, recip) \
    (((dir) << 7) | ((type) << 5) | (recip))

#define USB_DT_DEVICE_SIZE               18
#define USB_DT_CONFIG_SIZE               9
#define USB_DT_INTERFACE_SIZE            9
#define USB_DT_ENDPOINT_SIZE             7
#define USB_DT_STRING_SIZE               2

// USB Device States
#define USB_STATE_NOTATTACHED            0
#define USB_STATE_ATTACHED               1
#define USB_STATE_POWERED                2
#define USB_STATE_DEFAULT                3
#define USB_STATE_ADDRESS                4
#define USB_STATE_CONFIGURED             5
#define USB_STATE_SUSPENDED              6

// USB Hub Descriptor Types
#define USB_DT_HUB                       0x29
#define USB_DT_SS_HUB                    0x2A

// Hub port feature selectors (wValue for SetPortFeature / ClearPortFeature)
#define USB_PORT_FEAT_ENABLE             1
#define USB_PORT_FEAT_SUSPEND            2
#define USB_PORT_FEAT_RESET              4
#define USB_PORT_FEAT_POWER              8
#define USB_PORT_FEAT_C_CONNECTION       16
#define USB_PORT_FEAT_C_ENABLE           17
#define USB_PORT_FEAT_C_SUSPEND          18
#define USB_PORT_FEAT_C_OVER_CURRENT     19
#define USB_PORT_FEAT_C_RESET            20
// SuperSpeed-only feature selectors
#define USB_PORT_FEAT_C_PORT_LINK_STATE  25
#define USB_PORT_FEAT_C_BH_PORT_RESET    29

// Hub port status bits (wPortStatus field from GetPortStatus)
#define USB_PORT_STAT_CONNECTION         (1 << 0)
#define USB_PORT_STAT_ENABLE             (1 << 1)
#define USB_PORT_STAT_SUSPEND            (1 << 2)
#define USB_PORT_STAT_OVERCURRENT        (1 << 3)
#define USB_PORT_STAT_RESET              (1 << 4)
#define USB_PORT_STAT_POWER              (1 << 8)
#define USB_PORT_STAT_LOW_SPEED          (1 << 9)
#define USB_PORT_STAT_HIGH_SPEED         (1 << 10)

// Hub port change bits (wPortChange field from GetPortStatus)
#define USB_PORT_CHANGE_CONNECTION       (1 << 0)
#define USB_PORT_CHANGE_ENABLE           (1 << 1)
#define USB_PORT_CHANGE_SUSPEND          (1 << 2)
#define USB_PORT_CHANGE_OVERCURRENT      (1 << 3)
#define USB_PORT_CHANGE_RESET            (1 << 4)
// SuperSpeed-only change bits
#define USB_PORT_CHANGE_BH_RESET         (1 << 5)
#define USB_PORT_CHANGE_LINK_STATE       (1 << 6)

// USB Hub Descriptor (USB 2.0 spec section 11.23.2.1)
typedef struct usb_hub_descriptor {
    uint8_t  bLength;
    uint8_t  bDescriptorType;       /* USB_DT_HUB */
    uint8_t  bNbrPorts;
    uint16_t wHubCharacteristics;
    uint8_t  bPwrOn2PwrGood;        /* in 2ms units */
    uint8_t  bHubContrCurrent;
    uint8_t  DeviceRemovable[2];    /* covers up to 15 ports */
} __attribute__((packed)) usb_hub_descriptor_t;

// USB SuperSpeed Hub Descriptor (USB 3.x spec section 10.14.2)
typedef struct usb_ss_hub_descriptor {
    uint8_t  bLength;               /* 12 */
    uint8_t  bDescriptorType;       /* USB_DT_SS_HUB */
    uint8_t  bNbrPorts;
    uint16_t wHubCharacteristics;
    uint8_t  bPwrOn2PwrGood;        /* in 2ms units */
    uint8_t  bHubContrCurrent;
    uint8_t  bHubHdrDecLat;
    uint16_t wHubDelay;
    uint16_t DeviceRemovable;
} __attribute__((packed)) usb_ss_hub_descriptor_t;

// Port Status structure returned by GetPortStatus (hub class request)
typedef struct usb_port_status {
    uint16_t wPortStatus;
    uint16_t wPortChange;
} __attribute__((packed)) usb_port_status_t;


#endif /* __USB_H__ */