/*
 * EAPOL definitions shared between hostapd and wpa_supplicant
 * Copyright (c) 2002-2007, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef EAPOL_COMMON_H
#define EAPOL_COMMON_H

/* IEEE Std 802.1X-2004 */

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif /* _MSC_VER */

struct ieee802_1x_hdr {
	u8 version;
	u8 type;
	be16 length;
	/* followed by length octets of data */
} STRUCT_PACKED;

struct ieee8023_hdr {
	u8 dest[ETH_ALEN];
	u8 src[ETH_ALEN];
	u16 ethertype;
} STRUCT_PACKED;

#ifdef _MSC_VER
#pragma pack(pop)
#endif /* _MSC_VER */

#ifdef CONFIG_MACSEC
#define EAPOL_VERSION 3
#else /* CONFIG_MACSEC */
#define EAPOL_VERSION 2
#endif /* CONFIG_MACSEC */

enum { IEEE802_1X_TYPE_EAP_PACKET = 0,
       IEEE802_1X_TYPE_EAPOL_START = 1,
       IEEE802_1X_TYPE_EAPOL_LOGOFF = 2,
       IEEE802_1X_TYPE_EAPOL_KEY = 3,
       IEEE802_1X_TYPE_EAPOL_ENCAPSULATED_ASF_ALERT = 4,
       IEEE802_1X_TYPE_EAPOL_MKA = 5,
};

enum { EAPOL_KEY_TYPE_RC4 = 1, EAPOL_KEY_TYPE_RSN = 2,
       EAPOL_KEY_TYPE_WPA = 254 };

#endif /* EAPOL_COMMON_H */