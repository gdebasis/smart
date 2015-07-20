#ifndef HN_UNICODE_H
#define HN_UNICODE_H

#define HN_RANGE_START  0x0900
#define HN_RANGE_END    0x097F

#define hn_chandrabindu 0x0901
#define hn_anusvara     0x0902
#define hn_visarga      0x0903

#define hn_a            0x0905
#define hn_aa           0x0906
#define hn_i            0x0907
#define hn_ii           0x0908
#define hn_u            0x0909
#define hn_uu           0x090a
#define hn_e            0x090f
#define hn_ai           0x0910
#define hn_o            0x0913
#define hn_au           0x0914

#define hn_k            0x0915
#define hn_kh           0x0916
#define hn_g            0x0917
#define hn_gh           0x0918
#define hn_ng           0x0919
#define hn_ch           0x091a
#define hn_chh          0x091b
#define hn_j            0x091c
#define hn_jh           0x091d
#define hn_n            0x091e
#define hn_tt           0x091f
#define hn_tth          0x0920
#define hn_dd           0x0921
#define hn_ddh          0x0922
#define hn_mn           0x0923
#define hn_t            0x0924
#define hn_th           0x0925
#define hn_d            0x0926
#define hn_dh           0x0927
#define hn_dn           0x0928
#define hn_p            0x092a
#define hn_ph           0x092b
#define hn_b            0x092c
#define hn_bh           0x092d
#define hn_m            0x092e
#define hn_y            0x092f
#define hn_J            0x092f
#define hn_r            0x0930
#define hn_l            0x0932
#define hn_sh           0x0936
#define hn_ss           0x0937
#define hn_s            0x0938
#define hn_h            0x0939

/* matra */
#define hn_AA           0x093e
#define hn_I            0x093f
#define hn_II           0x0940
#define hn_U            0x0941
#define hn_UU           0x0942
#define hn_RI           0x0943
#define hn_E            0x0947
#define hn_AI           0x0948
#define hn_O            0x094b
#define hn_AU           0x094c

#define hn_hosh         0x094d
#define hn_nukta        0x093c
#define hn_virama       0x0964

/* numerals */
#define hn_zero         0x0966
#define hn_one          0x0967
#define hn_two          0x0968
#define hn_three        0x0969
#define hn_four         0x096a
#define hn_five         0x096b
#define hn_six          0x096c
#define hn_seven        0x096d
#define hn_eight        0x096e
#define hn_nine         0x096f

#define isHnConsonant(x) ((x) >= 0x0915 && (x) <= 0x0939)
#define isHnVowel(x) ((x) >= 0x0905 && (x) <= 0x0914)
#define isHnMatra(x) ((x) >= 0x093e && (x) <= 0x094c)

#endif
