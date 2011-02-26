/*
   Control is such a trick--

   We can guide the ship
   but are we ever really in control
   of where we land?
*/

/* Copyright 2004-2011 Jan Pekau
   
   This file is part of Freewheeling.
   
   Freewheeling is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 2 of the License, or
   (at your option) any later version.
   
   Freewheeling is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with Freewheeling.  If not, see <http://www.gnu.org/licenses/>. */

/* This file contains code from amixer.c, ALSA's command line mixer utility:
 *
 *   ALSA command line mixer utility
 *   Copyright (c) 1999-2000 by Jaroslav Kysela <perex@perex.cz>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include "fweelin_amixer.h"

#include <alsa/asoundlib.h>

#define NO_CHECK 0

#define LEVEL_BASIC     (1<<0)
#define LEVEL_INACTIVE  (1<<1)
#define LEVEL_ID        (1<<2)

const char *HardwareMixerInterface::control_type(snd_ctl_elem_info_t *info)
{
    return snd_ctl_elem_type_name(snd_ctl_elem_info_get_type(info));
}

const char *HardwareMixerInterface::control_access(snd_ctl_elem_info_t *info)
{
    static char result[10];
    char *res = result;

    *res++ = snd_ctl_elem_info_is_readable(info) ? 'r' : '-';
    *res++ = snd_ctl_elem_info_is_writable(info) ? 'w' : '-';
    *res++ = snd_ctl_elem_info_is_inactive(info) ? 'i' : '-';
    *res++ = snd_ctl_elem_info_is_volatile(info) ? 'v' : '-';
    *res++ = snd_ctl_elem_info_is_locked(info) ? 'l' : '-';
    *res++ = snd_ctl_elem_info_is_tlv_readable(info) ? 'R' : '-';
    *res++ = snd_ctl_elem_info_is_tlv_writable(info) ? 'W' : '-';
    *res++ = snd_ctl_elem_info_is_tlv_commandable(info) ? 'C' : '-';
    *res++ = '\0';
    return result;
}

#define convert_prange1(val, min, max) \
    ceil((val) * ((max) - (min)) * 0.01 + (min))
#define check_range(val, min, max) \
    (NO_CHECK ? (val) : ((val < min) ? (min) : (val > max) ? (max) : (val)))

long HardwareMixerInterface::get_integer(char **ptr, long min, long max)
{
    long val = min;
    char *p = *ptr, *s;

    if (*p == ':')
        p++;
    if (*p == '\0' || (!isdigit(*p) && *p != '-'))
        goto out;

    s = p;
    val = strtol(s, &p, 10);
    if (*p == '.') {
        p++;
        strtol(p, &p, 10);
    }
    if (*p == '%') {
        val = (long)convert_prange1(strtod(s, NULL), min, max);
        p++;
    }
    val = check_range(val, min, max);
    if (*p == ',')
        p++;
 out:
    *ptr = p;
    return val;
}

long HardwareMixerInterface::get_integer64(char **ptr, long long min, long long max)
{
    long long val = min;
    char *p = *ptr, *s;

    if (*p == ':')
        p++;
    if (*p == '\0' || (!isdigit(*p) && *p != '-'))
        goto out;

    s = p;
    val = strtol(s, &p, 10);
    if (*p == '.') {
        p++;
        strtol(p, &p, 10);
    }
    if (*p == '%') {
        val = (long long)convert_prange1(strtod(s, NULL), min, max);
        p++;
    }
    val = check_range(val, min, max);
    if (*p == ',')
        p++;
 out:
    *ptr = p;
    return val;
}

int HardwareMixerInterface::parse_control_id(const char *str, snd_ctl_elem_id_t *id)
{
    int c, size, numid;
    char *ptr;

    while (*str == ' ' || *str == '\t')
        str++;
    if (!(*str))
        return -EINVAL;
    snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);    /* default */
    while (*str) {
        if (!strncasecmp(str, "numid=", 6)) {
            str += 6;
            numid = atoi(str);
            if (numid <= 0) {
                printf( "amixer: Invalid numid %d\n", numid);
                return -EINVAL;
            }
            snd_ctl_elem_id_set_numid(id, atoi(str));
            while (isdigit(*str))
                str++;
        } else if (!strncasecmp(str, "iface=", 6)) {
            str += 6;
            if (!strncasecmp(str, "card", 4)) {
                snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_CARD);
                str += 4;
            } else if (!strncasecmp(str, "mixer", 5)) {
                snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_MIXER);
                str += 5;
            } else if (!strncasecmp(str, "pcm", 3)) {
                snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_PCM);
                str += 3;
            } else if (!strncasecmp(str, "rawmidi", 7)) {
                snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_RAWMIDI);
                str += 7;
            } else if (!strncasecmp(str, "timer", 5)) {
                snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_TIMER);
                str += 5;
            } else if (!strncasecmp(str, "sequencer", 9)) {
                snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_SEQUENCER);
                str += 9;
            } else {
                return -EINVAL;
            }
        } else if (!strncasecmp(str, "name=", 5)) {
            char buf[64];
            str += 5;
            ptr = buf;
            size = 0;
            if (*str == '\'' || *str == '\"') {
                c = *str++;
                while (*str && *str != c) {
                    if (size < (int)sizeof(buf)) {
                        *ptr++ = *str;
                        size++;
                    }
                    str++;
                }
                if (*str == c)
                    str++;
            } else {
                while (*str && *str != ',') {
                    if (size < (int)sizeof(buf)) {
                        *ptr++ = *str;
                        size++;
                    }
                    str++;
                }
                *ptr = '\0';
            }
            snd_ctl_elem_id_set_name(id, buf);
        } else if (!strncasecmp(str, "index=", 6)) {
            str += 6;
            snd_ctl_elem_id_set_index(id, atoi(str));
            while (isdigit(*str))
                str++;
        } else if (!strncasecmp(str, "device=", 7)) {
            str += 7;
            snd_ctl_elem_id_set_device(id, atoi(str));
            while (isdigit(*str))
                str++;
        } else if (!strncasecmp(str, "subdevice=", 10)) {
            str += 10;
            snd_ctl_elem_id_set_subdevice(id, atoi(str));
            while (isdigit(*str))
                str++;
        }
        if (*str == ',') {
            str++;
        } else {
            if (*str)
                return -EINVAL;
        }
    }
    return 0;
}

const char *HardwareMixerInterface::control_iface(snd_ctl_elem_id_t *id)
{
    return snd_ctl_elem_iface_name(snd_ctl_elem_id_get_interface(id));
}

void HardwareMixerInterface::show_control_id(snd_ctl_elem_id_t *id)
{
    unsigned int index, device, subdevice;
    printf("numid=%u,iface=%s,name='%s'",
           snd_ctl_elem_id_get_numid(id),
           control_iface(id),
           snd_ctl_elem_id_get_name(id));
    index = snd_ctl_elem_id_get_index(id);
    device = snd_ctl_elem_id_get_device(id);
    subdevice = snd_ctl_elem_id_get_subdevice(id);
    if (index)
        printf(",index=%i", index);
    if (device)
        printf(",device=%i", device);
    if (subdevice)
        printf(",subdevice=%i", subdevice);
}

void HardwareMixerInterface::print_spaces(unsigned int spaces)
{
    while (spaces-- > 0)
        putc(' ', stdout);
}

void HardwareMixerInterface::print_dB(long dB)
{
    printf("%li.%02lidB", dB / 100, (dB < 0 ? -dB : dB) % 100);
}

void HardwareMixerInterface::decode_tlv(unsigned int spaces, unsigned int *tlv, unsigned int tlv_size)
{
    unsigned int type = tlv[0];
    unsigned int size;
    unsigned int idx = 0;

    if (tlv_size < 2 * sizeof(unsigned int)) {
        printf("TLV size error!\n");
        return;
    }
    print_spaces(spaces);
    printf("| ");
    type = tlv[idx++];
    size = tlv[idx++];
    tlv_size -= 2 * sizeof(unsigned int);
    if (size > tlv_size) {
        printf("TLV size error (%i, %i, %i)!\n", type, size, tlv_size);
        return;
    }
    switch (type) {
    case SND_CTL_TLVT_CONTAINER:
        size += sizeof(unsigned int) -1;
        size /= sizeof(unsigned int);
        while (idx < size) {
            if (tlv[idx+1] > (size - idx) * sizeof(unsigned int)) {
                printf("TLV size error in compound!\n");
                return;
            }
            decode_tlv(spaces + 2, tlv + idx, tlv[idx+1]);
            idx += 2 + (tlv[1] + sizeof(unsigned int) - 1) / sizeof(unsigned int);
        }
        break;
    case SND_CTL_TLVT_DB_SCALE:
        printf("dBscale-");
        if (size != 2 * sizeof(unsigned int)) {
            while (size > 0) {
                printf("0x%08x,", tlv[idx++]);
                size -= sizeof(unsigned int);
            }
        } else {
            printf("min=");
            print_dB((int)tlv[2]);
            printf(",step=");
            print_dB(tlv[3] & 0xffff);
            printf(",mute=%i", (tlv[3] >> 16) & 1);
        }
        break;
#ifdef SND_CTL_TLVT_DB_LINEAR
    case SND_CTL_TLVT_DB_LINEAR:
        printf("dBlinear-");
        if (size != 2 * sizeof(unsigned int)) {
            while (size > 0) {
                printf("0x%08x,", tlv[idx++]);
                size -= sizeof(unsigned int);
            }
        } else {
            printf("min=");
            print_dB(tlv[2]);
            printf(",max=");
            print_dB(tlv[3]);
        }
        break;
#endif
#ifdef SND_CTL_TLVT_DB_RANGE
    case SND_CTL_TLVT_DB_RANGE:
        printf("dBrange-\n");
        if ((size / (6 * sizeof(unsigned int))) != 0) {
            while (size > 0) {
                printf("0x%08x,", tlv[idx++]);
                size -= sizeof(unsigned int);
            }
            break;
        }
        idx = 0;
        while (idx < size) {
            print_spaces(spaces + 2);
            printf("rangemin=%i,", tlv[0]);
            printf(",rangemax=%i\n", tlv[1]);
            decode_tlv(spaces + 4, tlv + 2, 6 * sizeof(unsigned int));
            idx += 6 * sizeof(unsigned int);
        }
        break;
#endif
#ifdef SND_CTL_TLVT_DB_MINMAX
    case SND_CTL_TLVT_DB_MINMAX:
    case SND_CTL_TLVT_DB_MINMAX_MUTE:
        if (type == SND_CTL_TLVT_DB_MINMAX_MUTE)
            printf("dBminmaxmute-");
        else
            printf("dBminmax-");
        if (size != 2 * sizeof(unsigned int)) {
            while (size > 0) {
                printf("0x%08x,", tlv[idx++]);
                size -= sizeof(unsigned int);
            }
        } else {
            printf("min=");
            print_dB(tlv[2]);
            printf(",max=");
            print_dB(tlv[3]);
        }
        break;
#endif
    default:
        printf("unk-%i-", type);
        while (size > 0) {
            printf("0x%08x,", tlv[idx++]);
            size -= sizeof(unsigned int);
        }
        break;
    }
    putc('\n', stdout);
}

int HardwareMixerInterface::show_control(char *card, const char *space, snd_hctl_elem_t *elem,
            int level)
{
    int err;
    unsigned int item, idx, count, *tlv;
    snd_ctl_elem_type_t type;
    snd_ctl_elem_id_t *id;
    snd_ctl_elem_info_t *info;
    snd_ctl_elem_value_t *control;
    snd_aes_iec958_t iec958;
    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_info_alloca(&info);
    snd_ctl_elem_value_alloca(&control);
    if ((err = snd_hctl_elem_info(elem, info)) < 0) {
        printf("Control %s snd_hctl_elem_info error: %s\n", card, snd_strerror(err));
        return err;
    }
    if (level & LEVEL_ID) {
        snd_hctl_elem_get_id(elem, id);
        show_control_id(id);
        printf("\n");
    }
    count = snd_ctl_elem_info_get_count(info);
    type = snd_ctl_elem_info_get_type(info);
    printf("%s; type=%s,access=%s,values=%i", space, control_type(info), control_access(info), count);
    switch (type) {
    case SND_CTL_ELEM_TYPE_INTEGER:
        printf(",min=%li,max=%li,step=%li\n",
               snd_ctl_elem_info_get_min(info),
               snd_ctl_elem_info_get_max(info),
               snd_ctl_elem_info_get_step(info));
        break;
    case SND_CTL_ELEM_TYPE_INTEGER64:
        printf(",min=%Li,max=%Li,step=%Li\n",
               snd_ctl_elem_info_get_min64(info),
               snd_ctl_elem_info_get_max64(info),
               snd_ctl_elem_info_get_step64(info));
        break;
    case SND_CTL_ELEM_TYPE_ENUMERATED:
    {
        unsigned int items = snd_ctl_elem_info_get_items(info);
        printf(",items=%u\n", items);
        for (item = 0; item < items; item++) {
            snd_ctl_elem_info_set_item(info, item);
            if ((err = snd_hctl_elem_info(elem, info)) < 0) {
                printf("Control %s element info error: %s\n", card, snd_strerror(err));
                return err;
            }
            printf("%s; Item #%u '%s'\n", space, item, snd_ctl_elem_info_get_item_name(info));
        }
        break;
    }
    default:
        printf("\n");
        break;
    }
    if (level & LEVEL_BASIC) {
        if (!snd_ctl_elem_info_is_readable(info))
            goto __skip_read;
        if ((err = snd_hctl_elem_read(elem, control)) < 0) {
            printf("Control %s element read error: %s\n", card, snd_strerror(err));
            return err;
        }
        printf("%s: values=", space);
        for (idx = 0; idx < count; idx++) {
            if (idx > 0)
                printf(",");
            switch (type) {
            case SND_CTL_ELEM_TYPE_BOOLEAN:
                printf("%s", snd_ctl_elem_value_get_boolean(control, idx) ? "on" : "off");
                break;
            case SND_CTL_ELEM_TYPE_INTEGER:
                printf("%li", snd_ctl_elem_value_get_integer(control, idx));
                break;
            case SND_CTL_ELEM_TYPE_INTEGER64:
                printf("%Li", snd_ctl_elem_value_get_integer64(control, idx));
                break;
            case SND_CTL_ELEM_TYPE_ENUMERATED:
                printf("%u", snd_ctl_elem_value_get_enumerated(control, idx));
                break;
            case SND_CTL_ELEM_TYPE_BYTES:
                printf("0x%02x", snd_ctl_elem_value_get_byte(control, idx));
                break;
            case SND_CTL_ELEM_TYPE_IEC958:
                snd_ctl_elem_value_get_iec958(control, &iec958);
                printf("[AES0=0x%02x AES1=0x%02x AES2=0x%02x AES3=0x%02x]",
                       iec958.status[0], iec958.status[1],
                       iec958.status[2], iec958.status[3]);
                break;
            default:
                printf("?");
                break;
            }
        }
        printf("\n");
          __skip_read:
        if (!snd_ctl_elem_info_is_tlv_readable(info))
            goto __skip_tlv;
        tlv = (unsigned int *) malloc(4096);
        if ((err = snd_hctl_elem_tlv_read(elem, tlv, 4096)) < 0) {
            printf("Control %s element TLV read error: %s\n", card, snd_strerror(err));
            free(tlv);
            return err;
        }
        decode_tlv(strlen(space), tlv, 4096);
        free(tlv);
    }
      __skip_tlv:
    return 0;
}

int HardwareMixerInterface::get_ctl_enum_item_index(snd_ctl_t *handle, snd_ctl_elem_info_t *info,
                   char **ptrp)
{
    char *ptr = *ptrp;
    int items, i, len;
    const char *name;

    items = snd_ctl_elem_info_get_items(info);
    if (items <= 0)
        return -1;

    for (i = 0; i < items; i++) {
        snd_ctl_elem_info_set_item(info, i);
        if (snd_ctl_elem_info(handle, info) < 0)
            return -1;
        name = snd_ctl_elem_info_get_item_name(info);
        len = strlen(name);
        if (! strncmp(name, ptr, len)) {
            if (! ptr[len] || ptr[len] == ',' || ptr[len] == '\n') {
                ptr += len;
                *ptrp = ptr;
                return i;
            }
        }
    }
    return -1;
}

int HardwareMixerInterface::cset(char *card, int argc, char *argv[], int roflag, int keep_handle, int debugflag)
{
    int err;
    static snd_ctl_t *handle = NULL;
    snd_ctl_elem_info_t *info;
    snd_ctl_elem_id_t *id;
    snd_ctl_elem_value_t *control;
    char *ptr;
    unsigned int idx, count;
    long tmp;
    snd_ctl_elem_type_t type;
    snd_ctl_elem_info_alloca(&info);
    snd_ctl_elem_id_alloca(&id);
    snd_ctl_elem_value_alloca(&control);

    if (argc < 1) {
        printf( "Specify a full control identifier: [[iface=<iface>,][name='name',][index=<index>,][device=<device>,][subdevice=<subdevice>]]|[numid=<numid>]\n");
        return -EINVAL;
    }
    if (parse_control_id(argv[0], id)) {
        printf( "Wrong control identifier: %s\n", argv[0]);
        return -EINVAL;
    }
    if (debugflag) {
        printf("VERIFY ID: ");
        show_control_id(id);
        printf("\n");
    }
    if (handle == NULL &&
        (err = snd_ctl_open(&handle, card, 0)) < 0) {
        printf("Control %s open error: %s\n", card, snd_strerror(err));
        return err;
    }
    snd_ctl_elem_info_set_id(info, id);
    if ((err = snd_ctl_elem_info(handle, info)) < 0) {
        printf("Cannot find the given element from control %s\n", card);
        if (! keep_handle) {
            snd_ctl_close(handle);
            handle = NULL;
        }
        return err;
    }
    snd_ctl_elem_info_get_id(info, id); /* FIXME: Remove it when hctl find works ok !!! */
    type = snd_ctl_elem_info_get_type(info);
    count = snd_ctl_elem_info_get_count(info);
    snd_ctl_elem_value_set_id(control, id);

    if (!roflag) {
        ptr = argv[1];
        for (idx = 0; idx < count && idx < 128 && ptr && *ptr; idx++) {
            switch (type) {
            case SND_CTL_ELEM_TYPE_BOOLEAN:
                tmp = 0;
                if (!strncasecmp(ptr, "on", 2) || !strncasecmp(ptr, "up", 2)) {
                    tmp = 1;
                    ptr += 2;
                } else if (!strncasecmp(ptr, "yes", 3)) {
                    tmp = 1;
                    ptr += 3;
                } else if (!strncasecmp(ptr, "toggle", 6)) {
                    tmp = snd_ctl_elem_value_get_boolean(control, idx);
                    tmp = tmp > 0 ? 0 : 1;
                    ptr += 6;
                } else if (isdigit(*ptr)) {
                    tmp = atoi(ptr) > 0 ? 1 : 0;
                    while (isdigit(*ptr))
                        ptr++;
                } else {
                    while (*ptr && *ptr != ',')
                        ptr++;
                }
                snd_ctl_elem_value_set_boolean(control, idx, tmp);
                break;
            case SND_CTL_ELEM_TYPE_INTEGER:
                tmp = get_integer(&ptr,
                          snd_ctl_elem_info_get_min(info),
                          snd_ctl_elem_info_get_max(info));
                snd_ctl_elem_value_set_integer(control, idx, tmp);
                break;
            case SND_CTL_ELEM_TYPE_INTEGER64:
                tmp = get_integer64(&ptr,
                          snd_ctl_elem_info_get_min64(info),
                          snd_ctl_elem_info_get_max64(info));
                snd_ctl_elem_value_set_integer64(control, idx, tmp);
                break;
            case SND_CTL_ELEM_TYPE_ENUMERATED:
                tmp = get_ctl_enum_item_index(handle, info, &ptr);
                if (tmp < 0)
                    tmp = get_integer(&ptr, 0, snd_ctl_elem_info_get_items(info) - 1);
                snd_ctl_elem_value_set_enumerated(control, idx, tmp);
                break;
            case SND_CTL_ELEM_TYPE_BYTES:
                tmp = get_integer(&ptr, 0, 255);
                snd_ctl_elem_value_set_byte(control, idx, tmp);
                break;
            default:
                break;
            }
            if (!strchr(argv[1], ','))
                ptr = argv[1];
            else if (*ptr == ',')
                ptr++;
        }
        if ((err = snd_ctl_elem_write(handle, control)) < 0) {
            printf("Control %s element write error: %s\n", card, snd_strerror(err));
            if (!keep_handle) {
                snd_ctl_close(handle);
                handle = NULL;
            }
            return err;
        }
    }
    if (! keep_handle) {
        snd_ctl_close(handle);
        handle = NULL;
    }
    if (debugflag) {
        snd_hctl_t *hctl;
        snd_hctl_elem_t *elem;
        if ((err = snd_hctl_open(&hctl, card, 0)) < 0) {
            printf("Control %s open error: %s\n", card, snd_strerror(err));
            return err;
        }
        if ((err = snd_hctl_load(hctl)) < 0) {
            printf("Control %s load error: %s\n", card, snd_strerror(err));
            return err;
        }
        elem = snd_hctl_find_elem(hctl, id);
        if (elem)
            show_control(card, "  ", elem, LEVEL_BASIC | LEVEL_ID);
        else
            printf("Could not find the specified element\n");
        snd_hctl_close(hctl);
    }
    return 0;
}

int HardwareMixerInterface::ALSAMixerControlSet(int hwid, int numid, int val1, int val2, int val3, int val4) {
  // Generate amixer style control string based on IDs
  char numid_str[256],
    val_str[256];

  if (numid < 0) {
    printf("AMIXER: Invalid ALSA mixer setting - no numid specified.\n");
    return -1;
  }
  snprintf(numid_str,255,"numid=%d",numid);

  if (val4 != -1)
    snprintf(val_str,255,"%d,%d,%d,%d",
        val1,val2,val3,val4);
  else if (val3 != -1)
    snprintf(val_str,255,"%d,%d,%d",
        val1,val2,val3);
  else if (val2 != -1)
    snprintf(val_str,255,"%d,%d",
        val1,val2);
  else if (val1 != -1)
    snprintf(val_str,255,"%d",
        val1);
  else {
    printf("AMIXER: Invalid ALSA mixer setting - no control values specified.\n");
    return -1;
  }

  char cardstr[256];
  snprintf(cardstr,255,"hw:%d",hwid);

  char *cset_args[] = {numid_str,val_str};

  if (hwid != prev_hwid) {
    prev_hwid = hwid;
    if (CRITTERS)
      printf("AMIXER: Same card (hw:%d), optimizing\n",hwid);
    return cset(cardstr,2,cset_args,0,0,CRITTERS);
  } else
    // Same hwid as last time, don't reopen hctl - optimize
    return cset(cardstr,2,cset_args,0,1,CRITTERS);
}

