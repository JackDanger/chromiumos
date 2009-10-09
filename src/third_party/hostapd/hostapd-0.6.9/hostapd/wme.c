/*
 * hostapd / WMM (Wi-Fi Multimedia)
 * Copyright 2002-2003, Instant802 Networks, Inc.
 * Copyright 2005-2006, Devicescape Software, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "includes.h"

#include "hostapd.h"
#include "ieee802_11.h"
#include "wme.h"
#include "sta_info.h"
#include "driver.h"


/* TODO: maintain separate sequence and fragment numbers for each AC
 * TODO: IGMP snooping to track which multicasts to forward - and use QOS-DATA
 * if only WME stations are receiving a certain group */


static u8 wme_oui[3] = { 0x00, 0x50, 0xf2 };


/* Add WME Parameter Element to Beacon and Probe Response frames. */
u8 * hostapd_eid_wme(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;
	struct wme_parameter_element *wme =
		(struct wme_parameter_element *) (pos + 2);
	int e;

	if (!hapd->conf->wme_enabled)
		return eid;
	eid[0] = WLAN_EID_VENDOR_SPECIFIC;
	wme->oui[0] = 0x00;
	wme->oui[1] = 0x50;
	wme->oui[2] = 0xf2;
	wme->oui_type = WME_OUI_TYPE;
	wme->oui_subtype = WME_OUI_SUBTYPE_PARAMETER_ELEMENT;
	wme->version = WME_VERSION;
	wme->acInfo = hapd->parameter_set_count & 0xf;

	/* fill in a parameter set record for each AC */
	for (e = 0; e < 4; e++) {
		struct wme_ac_parameter *ac = &wme->ac[e];
		struct hostapd_wme_ac_params *acp =
			&hapd->iconf->wme_ac_params[e];

		ac->aifsn = acp->aifs;
		ac->acm = acp->admission_control_mandatory;
		ac->aci = e;
		ac->reserved = 0;
		ac->eCWmin = acp->cwmin;
		ac->eCWmax = acp->cwmax;
		ac->txopLimit = host_to_le16(acp->txopLimit);
	}

	pos = (u8 *) (wme + 1);
	eid[1] = pos - eid - 2; /* element length */

	return pos;
}


/* This function is called when a station sends an association request with
 * WME info element. The function returns zero on success or non-zero on any
 * error in WME element. eid does not include Element ID and Length octets. */
int hostapd_eid_wme_valid(struct hostapd_data *hapd, u8 *eid, size_t len)
{
	struct wme_information_element *wme;

	wpa_hexdump(MSG_MSGDUMP, "WME IE", eid, len);

	if (len < sizeof(struct wme_information_element)) {
		wpa_printf(MSG_DEBUG, "Too short WME IE (len=%lu)",
			   (unsigned long) len);
		return -1;
	}

	wme = (struct wme_information_element *) eid;
	wpa_printf(MSG_DEBUG, "Validating WME IE: OUI %02x:%02x:%02x  "
		   "OUI type %d  OUI sub-type %d  version %d",
		   wme->oui[0], wme->oui[1], wme->oui[2], wme->oui_type,
		   wme->oui_subtype, wme->version);
	if (os_memcmp(wme->oui, wme_oui, sizeof(wme_oui)) != 0 ||
	    wme->oui_type != WME_OUI_TYPE ||
	    wme->oui_subtype != WME_OUI_SUBTYPE_INFORMATION_ELEMENT ||
	    wme->version != WME_VERSION) {
		wpa_printf(MSG_DEBUG, "Unsupported WME IE OUI/Type/Subtype/"
			   "Version");
		return -1;
	}

	return 0;
}


/* This function is called when a station sends an ACK frame for an AssocResp
 * frame (status=success) and the matching AssocReq contained a WME element.
 */
int hostapd_wme_sta_config(struct hostapd_data *hapd, struct sta_info *sta)
{
	/* update kernel STA data for WME related items (WLAN_STA_WPA flag) */
	if (sta->flags & WLAN_STA_WME)
		hostapd_sta_set_flags(hapd, sta->addr, sta->flags,
				      WLAN_STA_WME, ~0);
	else
		hostapd_sta_set_flags(hapd, sta->addr, sta->flags,
				      0, ~WLAN_STA_WME);

	return 0;
}


static void wme_send_action(struct hostapd_data *hapd, const u8 *addr,
			    const struct wme_tspec_info_element *tspec,
			    u8 action_code, u8 dialogue_token, u8 status_code)
{
	u8 buf[256];
	struct ieee80211_mgmt *m = (struct ieee80211_mgmt *) buf;
	struct wme_tspec_info_element *t =
		(struct wme_tspec_info_element *)
		m->u.action.u.wme_action.variable;
	int len;

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "action response - reason %d", status_code);
	os_memset(buf, 0, sizeof(buf));
	m->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					WLAN_FC_STYPE_ACTION);
	os_memcpy(m->da, addr, ETH_ALEN);
	os_memcpy(m->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(m->bssid, hapd->own_addr, ETH_ALEN);
	m->u.action.category = WLAN_ACTION_WMM;
	m->u.action.u.wme_action.action_code = action_code;
	m->u.action.u.wme_action.dialog_token = dialogue_token;
	m->u.action.u.wme_action.status_code = status_code;
	os_memcpy(t, tspec, sizeof(struct wme_tspec_info_element));
	len = ((u8 *) (t + 1)) - buf;

	if (hostapd_send_mgmt_frame(hapd, m, len, 0) < 0)
		perror("wme_send_action: send");
}


static void wme_setup_request(struct hostapd_data *hapd,
			      struct ieee80211_mgmt *mgmt,
			      struct wme_tspec_info_element *tspec, size_t len)
{
	u8 *end = ((u8 *) mgmt) + len;
	int medium_time, pps, duration;
	int up, psb, dir, tid;
	u16 val, surplus;

	if ((u8 *) (tspec + 1) > end) {
		wpa_printf(MSG_DEBUG, "WMM: TSPEC overflow in ADDTS Request");
		return;
	}

	wpa_printf(MSG_DEBUG, "WMM: ADDTS Request (Dialog Token %d) for TSPEC "
		   "from " MACSTR,
		   mgmt->u.action.u.wme_action.dialog_token,
		   MAC2STR(mgmt->sa));

	up = (tspec->ts_info[1] >> 3) & 0x07;
	psb = (tspec->ts_info[1] >> 2) & 0x01;
	dir = (tspec->ts_info[0] >> 5) & 0x03;
	tid = (tspec->ts_info[0] >> 1) & 0x0f;
	wpa_printf(MSG_DEBUG, "WMM: TS Info: UP=%d PSB=%d Direction=%d TID=%d",
		   up, psb, dir, tid);
	val = le_to_host16(tspec->nominal_msdu_size);
	wpa_printf(MSG_DEBUG, "WMM: Nominal MSDU Size: %d%s",
		   val & 0x7fff, val & 0x8000 ? " (fixed)" : "");
	wpa_printf(MSG_DEBUG, "WMM: Mean Data Rate: %u bps",
		   le_to_host32(tspec->mean_data_rate));
	wpa_printf(MSG_DEBUG, "WMM: Minimum PHY Rate: %u bps",
		   le_to_host32(tspec->minimum_phy_rate));
	val = le_to_host16(tspec->surplus_bandwidth_allowance);
	wpa_printf(MSG_DEBUG, "WMM: Surplus Bandwidth Allowance: %u.%04u",
		   val >> 13, 10000 * (val & 0x1fff) / 0x2000);

	val = le_to_host16(tspec->nominal_msdu_size);
	if (val == 0) {
		wpa_printf(MSG_DEBUG, "WMM: Invalid Nominal MSDU Size (0)");
		goto invalid;
	}
	/* pps = Ceiling((Mean Data Rate / 8) / Nominal MSDU Size) */
	pps = ((le_to_host32(tspec->mean_data_rate) / 8) + val - 1) / val;
	wpa_printf(MSG_DEBUG, "WMM: Packets-per-second estimate for TSPEC: %d",
		   pps);

	if (le_to_host32(tspec->minimum_phy_rate) < 1000000) {
		wpa_printf(MSG_DEBUG, "WMM: Too small Minimum PHY Rate");
		goto invalid;
	}

	duration = (le_to_host16(tspec->nominal_msdu_size) & 0x7fff) * 8 /
		(le_to_host32(tspec->minimum_phy_rate) / 1000000) +
		50 /* FIX: proper SIFS + ACK duration */;

	/* unsigned binary number with an implicit binary point after the
	 * leftmost 3 bits, i.e., 0x2000 = 1.0 */
	surplus = le_to_host16(tspec->surplus_bandwidth_allowance);
	if (surplus <= 0x2000) {
		wpa_printf(MSG_DEBUG, "WMM: Surplus Bandwidth Allowance not "
			   "greater than unity");
		goto invalid;
	}

	medium_time = surplus * pps * duration / 0x2000;
	wpa_printf(MSG_DEBUG, "WMM: Estimated medium time: %u", medium_time);

	/*
	 * TODO: store list of granted (and still active) TSPECs and check
	 * whether there is available medium time for this request. For now,
	 * just refuse requests that would by themselves take very large
	 * portion of the available bandwidth.
	 */
	if (medium_time > 750000) {
		wpa_printf(MSG_DEBUG, "WMM: Refuse TSPEC request for over "
			   "75%% of available bandwidth");
		wme_send_action(hapd, mgmt->sa, tspec,
				WME_ACTION_CODE_SETUP_RESPONSE,
				mgmt->u.action.u.wme_action.dialog_token,
				WME_SETUP_RESPONSE_STATUS_REFUSED);
		return;
	}

	/* Convert to 32 microseconds per second unit */
	tspec->medium_time = host_to_le16(medium_time / 32);

	wme_send_action(hapd, mgmt->sa, tspec, WME_ACTION_CODE_SETUP_RESPONSE,
			mgmt->u.action.u.wme_action.dialog_token,
			WME_SETUP_RESPONSE_STATUS_ADMISSION_ACCEPTED);
	return;

invalid:
	wme_send_action(hapd, mgmt->sa, tspec,
			WME_ACTION_CODE_SETUP_RESPONSE,
			mgmt->u.action.u.wme_action.dialog_token,
			WME_SETUP_RESPONSE_STATUS_INVALID_PARAMETERS);
}


void hostapd_wme_action(struct hostapd_data *hapd, struct ieee80211_mgmt *mgmt,
			size_t len)
{
	int action_code;
	int left = len - IEEE80211_HDRLEN - 4;
	u8 *pos = ((u8 *) mgmt) + IEEE80211_HDRLEN + 4;
	struct ieee802_11_elems elems;
	struct sta_info *sta = ap_get_sta(hapd, mgmt->sa);

	/* check that the request comes from a valid station */
	if (!sta ||
	    (sta->flags & (WLAN_STA_ASSOC | WLAN_STA_WME)) !=
	    (WLAN_STA_ASSOC | WLAN_STA_WME)) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "wme action received is not from associated wme"
			       " station");
		/* TODO: respond with action frame refused status code */
		return;
	}

	/* extract the tspec info element */
	if (ieee802_11_parse_elems(pos, left, &elems, 1) == ParseFailed) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "hostapd_wme_action - could not parse wme "
			       "action");
		/* TODO: respond with action frame invalid parameters status
		 * code */
		return;
	}

	if (!elems.wme_tspec ||
	    elems.wme_tspec_len != (sizeof(struct wme_tspec_info_element) - 2))
	{
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "hostapd_wme_action - missing or wrong length "
			       "tspec");
		/* TODO: respond with action frame invalid parameters status
		 * code */
		return;
	}

	/* TODO: check the request is for an AC with ACM set, if not, refuse
	 * request */

	action_code = mgmt->u.action.u.wme_action.action_code;
	switch (action_code) {
	case WME_ACTION_CODE_SETUP_REQUEST:
		wme_setup_request(hapd, mgmt, (struct wme_tspec_info_element *)
				  elems.wme_tspec, len);
		return;
#if 0
	/* TODO: needed for client implementation */
	case WME_ACTION_CODE_SETUP_RESPONSE:
		wme_setup_request(hapd, mgmt, len);
		return;
	/* TODO: handle station teardown requests */
	case WME_ACTION_CODE_TEARDOWN:
		wme_teardown(hapd, mgmt, len);
		return;
#endif
	}

	hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "hostapd_wme_action - unknown action code %d",
		       action_code);
}
