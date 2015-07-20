#ifndef BN_CTYPES_H
#define BN_CTYPES_H

#define BN_RANGE_START  0x0980
#define BN_RANGE_END    0x09FF

#define BN_UNDEF_T   0x0
#define BN_ALPHA_T   0x1
#define BN_DIGIT_T   0x2
#define BN_PUNCT_T   0x4
#define BN_INTERN_T  0x8
#define BN_OTHER_T   0x10


#define isbnalpha(x) (isalpha(x) || ((x >= BN_RANGE_START && x <= BN_RANGE_END) ? bn_ctypes[x - BN_RANGE_START] & BN_ALPHA_T : 0))
#define isbndigit(x) (isdigit(x) || (x >= BN_RANGE_START && x <= BN_RANGE_END) ? bn_ctypes[x - BN_RANGE_START] & BN_DIGIT_T : 0)
#define isbnpunct(x) (ispunct(x) || x == 0x0964 || ((x >= BN_RANGE_START && x <= BN_RANGE_END) ? bn_ctypes[x - BN_RANGE_START] & BN_PUNCT_T : 0))
#define isbnintern(x) ((x >= BN_RANGE_START && x <= BN_RANGE_END) ? bn_ctypes[x - BN_RANGE_START] & BN_INTERN_T : 0)


static unsigned char bn_ctypes[] = {
/*     bn_undef        0x0980 */ BN_UNDEF_T,
/*     bn_chandrabindu 0x0981 */ BN_ALPHA_T,
/*     bn_anusvara     0x0982 */ BN_ALPHA_T,
/*     bn_visarga      0x0983 */ BN_ALPHA_T,
/*     bn_undef        0x0984 */ BN_UNDEF_T,
/*     bn_a            0x0985 */ BN_ALPHA_T,
/*     bn_aa           0x0986 */ BN_ALPHA_T,
/*     bn_i            0x0987 */ BN_ALPHA_T,
/*     bn_ii           0x0988 */ BN_ALPHA_T,
/*     bn_u            0x0989 */ BN_ALPHA_T,
/*     bn_uu           0x098a */ BN_ALPHA_T,
/*     bn_ri           0x098b */ BN_ALPHA_T,
/*     bn_undef        0x098c */ BN_UNDEF_T,
/*     bn_undef        0x098d */ BN_UNDEF_T,
/*     bn_undef        0x098e */ BN_UNDEF_T,
/*     bn_e            0x098f */ BN_ALPHA_T,
/*     bn_ai           0x0990 */ BN_ALPHA_T,
/*     bn_undef        0x0991 */ BN_UNDEF_T,
/*     bn_undef        0x0992 */ BN_UNDEF_T,
/*     bn_o            0x0993 */ BN_ALPHA_T,
/*     bn_au           0x0994 */ BN_ALPHA_T,
/*     bn_k            0x0995 */ BN_ALPHA_T,
/*     bn_kh           0x0996 */ BN_ALPHA_T,
/*     bn_g            0x0997 */ BN_ALPHA_T,
/*     bn_gh           0x0998 */ BN_ALPHA_T,
/*     bn_ng           0x0999 */ BN_ALPHA_T,
/*     bn_ch           0x099a */ BN_ALPHA_T,
/*     bn_chh          0x099b */ BN_ALPHA_T,
/*     bn_j            0x099c */ BN_ALPHA_T,
/*     bn_jh           0x099d */ BN_ALPHA_T,
/*     bn_n            0x099e */ BN_ALPHA_T,
/*     bn_tt           0x099f */ BN_ALPHA_T,
/*     bn_tth          0x09a0 */ BN_ALPHA_T,
/*     bn_dd           0x09a1 */ BN_ALPHA_T,
/*     bn_ddh          0x09a2 */ BN_ALPHA_T,
/*     bn_mn           0x09a3 */ BN_ALPHA_T,
/*     bn_t            0x09a4 */ BN_ALPHA_T,
/*     bn_th           0x09a5 */ BN_ALPHA_T,
/*     bn_d            0x09a6 */ BN_ALPHA_T,
/*     bn_dh           0x09a7 */ BN_ALPHA_T,
/*     bn_dn           0x09a8 */ BN_ALPHA_T,
/*     bn_undef        0x09a9 */ BN_UNDEF_T,
/*     bn_p            0x09aa */ BN_ALPHA_T,
/*     bn_ph           0x09ab */ BN_ALPHA_T,
/*     bn_b            0x09ac */ BN_ALPHA_T,
/*     bn_bh           0x09ad */ BN_ALPHA_T,
/*     bn_m            0x09ae */ BN_ALPHA_T,
/*     bn_J            0x09af */ BN_ALPHA_T,
/*     bn_r            0x09b0 */ BN_ALPHA_T,
/*     bn_undef        0x09b1 */ BN_UNDEF_T,
/*     bn_l            0x09b2 */ BN_ALPHA_T,
/*     bn_undef        0x09b3 */ BN_UNDEF_T,
/*     bn_undef        0x09b4 */ BN_UNDEF_T,
/*     bn_undef        0x09b5 */ BN_UNDEF_T,
/*     bn_sh           0x09b6 */ BN_ALPHA_T,
/*     bn_ss           0x09b7 */ BN_ALPHA_T,
/*     bn_s            0x09b8 */ BN_ALPHA_T,
/*     bn_h            0x09b9 */ BN_ALPHA_T,
/*     bn_undef        0x09ba */ BN_UNDEF_T,
/*     bn_undef        0x09bb */ BN_UNDEF_T,
/*     bn_nukta        0x09bc */ BN_INTERN_T,
/*     bn_undef        0x09bd */ BN_OTHER_T,
/*     bn_AA           0x09be */ BN_ALPHA_T,
/*     bn_I            0x09bf */ BN_ALPHA_T,
/*     bn_II           0x09c0 */ BN_ALPHA_T,
/*     bn_U            0x09c1 */ BN_ALPHA_T,
/*     bn_UU           0x09c2 */ BN_ALPHA_T,
/*     bn_RI           0x09c3 */ BN_ALPHA_T,
/*     bn_undef        0x09c4 */ BN_OTHER_T,
/*     bn_undef        0x09c5 */ BN_UNDEF_T,
/*     bn_undef        0x09c6 */ BN_UNDEF_T,
/*     bn_E            0x09c7 */ BN_ALPHA_T,
/*     bn_AI           0x09c8 */ BN_ALPHA_T,
/*     bn_undef        0x09c9 */ BN_UNDEF_T,
/*     bn_undef        0x09ca */ BN_UNDEF_T,
/*     bn_O            0x09cb */ BN_ALPHA_T,
/*     bn_AU           0x09cc */ BN_ALPHA_T,
/*     bn_hosh         0x09cd */ BN_INTERN_T,
/*     bn_khandata     0x09ce */ BN_ALPHA_T,
/*     bn_undef        0x09cf */ BN_UNDEF_T,
/*     bn_undef        0x09d0 */ BN_UNDEF_T,
/*     bn_undef        0x09d1 */ BN_UNDEF_T,
/*     bn_undef        0x09d2 */ BN_UNDEF_T,
/*     bn_undef        0x09d3 */ BN_UNDEF_T,
/*     bn_undef        0x09d4 */ BN_UNDEF_T,
/*     bn_undef        0x09d5 */ BN_UNDEF_T,
/*     bn_undef        0x09d6 */ BN_UNDEF_T,
/*     bn_undef        0x09d7 */ BN_UNDEF_T,
/*     bn_undef        0x09d8 */ BN_UNDEF_T,
/*     bn_undef        0x09d9 */ BN_UNDEF_T,
/*     bn_undef        0x09da */ BN_UNDEF_T,
/*     bn_undef        0x09db */ BN_UNDEF_T,
/*     bn_rr           0x09dc */ BN_ALPHA_T,
/*     bn_rrh          0x09dd */ BN_ALPHA_T,
/*     bn_undef        0x09de */ BN_UNDEF_T,
/*     bn_y            0x09df */ BN_ALPHA_T,
/*     bn_undef        0x09e0 */ BN_UNDEF_T,
/*     bn_undef        0x09e1 */ BN_UNDEF_T,
/*     bn_undef        0x09e2 */ BN_UNDEF_T,
/*     bn_undef        0x09e3 */ BN_UNDEF_T,
/*     bn_undef        0x09e4 */ BN_UNDEF_T,
/*     bn_undef        0x09e5 */ BN_UNDEF_T,
/*     bn_zero         0x09e6 */ BN_DIGIT_T,
/*     bn_one          0x09e7 */ BN_DIGIT_T,
/*     bn_two          0x09e8 */ BN_DIGIT_T,
/*     bn_three        0x09e9 */ BN_DIGIT_T,
/*     bn_four         0x09ea */ BN_DIGIT_T,
/*     bn_five         0x09eb */ BN_DIGIT_T,
/*     bn_six          0x09ec */ BN_DIGIT_T,
/*     bn_seven        0x09ed */ BN_DIGIT_T,
/*     bn_eight        0x09ee */ BN_DIGIT_T,
/*     bn_nine         0x09ef */ BN_DIGIT_T,
/*     bn_undef        0x09f0 */ BN_UNDEF_T,
/*     bn_undef        0x09f1 */ BN_UNDEF_T,
/*     bn_undef        0x09f2 */ BN_OTHER_T,
/*     bn_undef        0x09f3 */ BN_OTHER_T,
/*     bn_undef        0x09f4 */ BN_OTHER_T,
/*     bn_undef        0x09f5 */ BN_OTHER_T,
/*     bn_undef        0x09f6 */ BN_OTHER_T,
/*     bn_undef        0x09f7 */ BN_OTHER_T,
/*     bn_undef        0x09f8 */ BN_OTHER_T,
/*     bn_undef        0x09f9 */ BN_OTHER_T,
/*     bn_undef        0x09fa */ BN_OTHER_T,
/*     bn_undef        0x09fb */ BN_UNDEF_T,
/*     bn_undef        0x09fc */ BN_UNDEF_T,
/*     bn_undef        0x09fd */ BN_UNDEF_T,
/*     bn_undef        0x09fe */ BN_UNDEF_T,
/*     bn_undef        0x09ff */ BN_UNDEF_T,
};

#endif
