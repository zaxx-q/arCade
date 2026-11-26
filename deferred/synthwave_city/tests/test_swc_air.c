/* The air traffic layer's data. The numbers marked "web version" were produced by the original TypeScript
 * (src/layers/airtraffic/pack.ts and shapes.ts) for the same seeds and densities, so they pin the port to it:
 * which craft fly in which sky lane (every random draw, in order), where the hero ships go, the closed-form
 * motion at a given moment, and the hull meshes vertex by vertex. The rest are properties the original's own
 * tests check: spacing, seam, tubes, closed and outward-wound hulls. */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "arcade_test.h"
#include "swc_air.h"

static SwcCity city;
static SwcCraftPack craft;
static SwcCraftPack craft2;
static SwcHeroPack heroes;
static SwcHull hull;
static SwcHeroLight lights[SWC_HERO_LIGHT_MAX];

#define LAP_SPEED (SWC_TILE / SWC_LOOP_SECONDS)

typedef struct GoldenState {
    int index;
    double loop, x, y, z, along, lateral;
} GoldenState;

typedef struct GoldenHeroState {
    int index;
    double loop, x, y, z, along;
} GoldenHeroState;

typedef struct GoldenCase {
    uint32_t seed;
    double density;
    uint32_t layer_seed;
    int sky_lanes, count;
    int shape_start[4];
    double sums[16], first[16], last[16];
    double lane_sum;
    int hero_count;
    double hero[48];
    int state_count;
    GoldenState states[12];
    int hero_state_count;
    GoldenHeroState hero_states[12];
} GoldenCase;

typedef struct GoldenHull {
    int vertices, indices, pieces, closed;
    double pos_sum, abs_sum, part_sum, type_sum, idx_sum;
    double bb[6];
    double first_pos[9], last_pos[9];
} GoldenHull;

typedef struct GoldenLights {
    int count;
    double sums[8];
    int by_type[3];
    double first[8], last[8];
} GoldenLights;

static const GoldenCase GOLDEN[] = {
    {1984u,
     1.0,
     272362444u,
     160,
     8613,
     {0, 5694, 6929, 8613},
     {27721874.228959084, 1973205.5562973022, 4299.0, 31.0, 258241.0, 4307.294027387348, 19924.435061097145,
      2893071.0, 9305.010765820742, 5178683.0, 553550.8232517242, 8761.077777445316, 4308.962158203125,
      4603.0, 16025.0, 21853.0},
     {10.3465576171875, 92.54412078857422, 0.0, 1.0, 19.0, 0.8936949968338013, 1.748717188835144, 443.0,
      1.4762861728668213, 707.0, 54.40973663330078, 1.1145737171173096, 0.809814453125, 0.0, 3.0, 1.0},
     {5768.1591796875, 405.1476135253906, 1.0, -1.0, 43.0, 0.40589115023612976, 1.886491060256958, 301.0,
      0.9596111178398132, 652.0, 69.74751281738281, 0.9805946350097656, 0.20263671875, 2.0, 5.0, 1.0},
     683453.0,
     6,
     {4480.0, 566.0, 1.0, 1.0,  6.0,  0.19924503564834595, 0.34765625,    0.0,
      640.0,  503.0, 1.0, -1.0, 4.0,  0.6193450093269348,  0.8828125,     1.0,
      5760.0, 450.0, 1.0, 1.0,  6.0,  0.4499560296535492,  0.1865234375,  0.0,
      3200.0, 467.0, 1.0, -1.0, 4.0,  0.16180381178855896, 0.77294921875, 1.0,
      5760.0, 190.0, 0.0, 1.0,  10.0, 0.6275776624679565,  0.32861328125, 2.0,
      5120.0, 235.0, 1.0, -1.0, 10.0, 0.21699905395507812, 0.53857421875, 2.0},
     12,
     {{0, 0.0, 5719.647979736328, 91.25628836387357, 8.719892104515088, 5719.647979736328,
       -1.6266655126724126},
      {0, 0.3, 3799.647979736329, 91.07801439094585, 8.653287749241713, 3799.647979736329,
       -1.6932698679457863},
      {0, 0.77, 3351.647979736333, 93.07635838781255, 9.502320733937166, 3351.647979736333,
       -0.8442368832503343},
      {2871, 0.0, 6388.404537070549, 134.58551221621363, 1339.1128540039062, 1339.1128540039062,
       0.31078707054923876},
      {2871, 0.3, 6386.648687880741, 133.00933641659887, 3899.1128540039026, 3899.1128540039026,
       -1.44506211925942},
      {2871, 0.77, 6390.13998212479, 133.18341099970584, 443.1128540039026, 443.1128540039026,
       2.0462321247895074},
      {4306, 0.0, 3192.164539941696, 133.09757277891555, 4304.00276184082, 4304.00276184082,
       2.4902235354459754},
      {4306, 0.3, 3191.0021503551766, 132.15521990563337, 4944.0027618408185, 4944.0027618408185,
       1.327833948926486},
      {4306, 0.77, 3192.2329938006937, 133.09451302369922, 4880.002761840819, 4880.002761840819,
       2.5586773944439427},
      {8612, 0.0, 5769.962750507373, 405.61804030901357, 2597.7033615112305, 2597.7033615112305,
       1.8035708198728846},
      {8612, 0.3, 5768.127927633882, 405.2586493427146, 3237.703361511228, 3237.703361511228,
       -0.031252053617701205},
      {8612, 0.77, 5767.836433516188, 405.3952586710333, 1893.703361511234, 1893.703361511234,
       -0.3227461713120929}},
     12,
     {{0, 0.0, 4480.0, 567.2263772197274, 1275.168228149414, 1275.168228149414},
      {0, 0.41, 4480.0, 564.6750412771364, 4219.168228149414, 4219.168228149414},
      {1, 0.0, 640.0, 500.98532313545894, 3963.808059692383, 3963.808059692383},
      {1, 0.41, 640.0, 505.996941328057, 6267.808059692384, 6267.808059692384},
      {2, 0.0, 5760.0, 451.3822710590131, 2879.718589782715, 2879.718589782715},
      {2, 0.41, 5760.0, 448.70163737181383, 5823.718589782715, 5823.718589782715},
      {3, 0.0, 3200.0, 464.0311339476104, 1035.5443954467773, 1035.5443954467773},
      {3, 0.41, 3200.0, 469.2245832220338, 3339.544395446778, 3339.544395446778},
      {4, 0.0, 4016.497039794922, 190.70437671124174, 5760.0, 4016.497039794922},
      {4, 0.41, 4656.49703979492, 190.77657056470895, 5760.0, 4656.49703979492},
      {5, 0.0, 5120.0, 234.807997582041, 1388.7939453125, 1388.7939453125},
      {5, 0.41, 5120.0, 235.00716666238154, 748.7939453125023, 748.7939453125023}}},
    {1u,
     1.0,
     1012092712u,
     160,
     8584,
     {0, 5648, 6862, 8584},
     {27466544.545050144, 1967394.459854126, 4293.0, 2.0, 258216.0, 4290.131051570484, 19924.249214410782,
      2895135.0, 9244.441706418991, 5152841.0, 550136.4031009674, 8741.718768060207, 4343.489501953125,
      4658.0, 15793.0, 21082.0},
     {9.489665031433105, 90.23522186279297, 0.0, 1.0, 20.0, 0.8256731033325195, 3.0452520847320557, 419.0,
      0.7753424644470215, 523.0, 42.8879508972168, 0.9501131772994995, 0.352783203125, 0.0, 3.0, 0.0},
     {5768.17236328125, 405.9788513183594, 1.0, -1.0, 44.0, 0.593901515007019, 2.391143798828125, 281.0,
      0.831810712814331, 628.0, 77.89420318603516, 0.9558129906654358, 0.211669921875, 2.0, 2.0, 2.0},
     680099.0,
     6,
     {5120.0, 586.0, 1.0, 1.0,  6.0,  0.11218462884426117, 0.210205078125, 0.0,
      1920.0, 507.0, 1.0, -1.0, 4.0,  0.3554548919200897,  0.985107421875, 1.0,
      2560.0, 518.0, 1.0, 1.0,  6.0,  0.29597020149230957, 0.863525390625, 0.0,
      3200.0, 467.0, 1.0, -1.0, 4.0,  0.11518797278404236, 0.870849609375, 1.0,
      1280.0, 190.0, 0.0, 1.0,  10.0, 0.7825537323951721,  0.365234375,    2.0,
      5760.0, 235.0, 1.0, -1.0, 10.0, 0.31167083978652954, 0.508056640625, 2.0},
     12,
     {{0, 0.0, 5284.307861328125, 90.3829825628567, 11.921649008742891, 5284.307861328125,
       2.4319839773097853},
      {0, 0.3, 5284.307861328125, 90.80214525042172, 10.481200284294204, 5284.307861328125,
       0.9915352528610987},
      {0, 0.77, 1444.307861328116, 90.93569541302885, 9.160882969293892, 1444.307861328116,
       -0.32878206213921385},
      {2861, 0.0, 6388.531378748783, 131.64438280671715, 4114.117431640625, 4114.117431640625,
       -1.415398594967236},
      {2861, 0.3, 6390.556048527275, 131.64438280671712, 2194.117431640626, 2194.117431640626,
       0.6092711835249873},
      {2861, 0.77, 6388.295199877724, 133.5281940975795, 1746.11743164063, 1746.11743164063,
       -1.6515774660253923},
      {4292, 0.0, 3205.792442021193, 126.98319487217363, 4316.529846191406, 4316.529846191406,
       -1.0647357131821211},
      {4292, 0.3, 3204.437775713131, 126.30552346765045, 6236.529846191405, 6236.529846191405,
       -2.419402021244149},
      {4292, 0.77, 3205.9879830678624, 127.12878808918714, 284.52984619140125, 284.52984619140125,
       -0.8691946665126873},
      {8583, 0.0, 5770.494496657776, 406.07433896808743, 3800.969696044922, 3800.969696044922,
       2.322133376526224},
      {8583, 0.3, 5767.997191445478, 405.4159063042962, 2520.9696960449264, 2520.9696960449264,
       -0.17517183577220882},
      {8583, 0.77, 5766.998499207418, 406.19425480422126, 4568.9696960449055, 4568.9696960449055,
       -1.1738640738318586}},
     12,
     {{0, 0.0, 5120.0, 587.4533542071647, 717.9816246032715, 717.9816246032715},
      {0, 0.41, 5120.0, 584.6046249122278, 3661.9816246032715, 3661.9816246032715},
      {1, 0.0, 1920.0, 506.71969099246274, 2274.911308288574, 2274.911308288574},
      {1, 0.41, 1920.0, 509.4801030205977, 4578.911308288575, 4578.911308288575},
      {2, 0.0, 2560.0, 516.8656909978785, 1894.2092895507812, 1894.2092895507812},
      {2, 0.41, 2560.0, 519.2483792590673, 4838.209289550781, 4838.209289550781},
      {3, 0.0, 3200.0, 464.8240880828808, 737.2030258178711, 737.2030258178711},
      {3, 0.41, 3200.0, 469.9783088677351, 3041.2030258178715, 3041.2030258178715},
      {4, 0.0, 5008.343887329102, 190.59930911561878, 1280.0, 5008.343887329102},
      {4, 0.41, 5648.3438873291, 190.71226960168735, 1280.0, 5648.3438873291},
      {5, 0.0, 5760.0, 234.95952020077047, 1994.693374633789, 1994.693374633789},
      {5, 0.41, 5760.0, 235.15948900286992, 1354.6933746337913, 1354.6933746337913}}},
    {42u,
     1.0,
     500443073u,
     160,
     8632,
     {0, 5565, 6840, 8632},
     {27578640.739791393, 1979062.2117385864, 4361.0, 126.0, 258876.0, 4309.94994020196, 19900.04770731926,
      2912396.0, 9282.634823471308, 5200043.0, 550585.9244842529, 8757.861721158028, 4268.98046875, 4859.0,
      16009.0, 22119.0},
     {5.765703201293945, 95.5594253540039, 0.0, 1.0, 19.0, 0.1397584080696106, 2.0465433597564697, 308.0,
      0.6176844835281372, 531.0, 34.05350875854492, 1.0934993028640747, 0.047119140625, 0.0, 3.0, 2.0},
     {5769.30224609375, 409.0899963378906, 1.0, -1.0, 44.0, 0.10337463766336441, 2.3184492588043213, 338.0,
      1.0576257705688477, 503.0, 60.7109260559082, 0.8187437057495117, 0.78125, 2.0, 2.0, 5.0},
     686703.0,
     6,
     {3840.0, 450.0, 0.0, 1.0,  6.0,  0.1826726347208023,  0.195556640625, 0.0,
      5120.0, 503.0, 0.0, -1.0, 4.0,  0.9987617135047913,  0.63232421875,  1.0,
      1920.0, 522.0, 0.0, 1.0,  6.0,  0.5913172960281372,  0.484130859375, 0.0,
      2560.0, 467.0, 0.0, -1.0, 4.0,  0.43682798743247986, 0.59765625,     1.0,
      3200.0, 190.0, 1.0, 1.0,  10.0, 0.4660970866680145,  0.51904296875,  2.0,
      4480.0, 235.0, 0.0, -1.0, 10.0, 0.28219112753868103, 0.96337890625,  2.0},
     12,
     {{0, 0.0, 894.4538116455078, 96.10101649772292, 6.362786858311434, 894.4538116455078,
       0.5970836570174889},
      {0, 0.3, 5374.453811645509, 95.10959404996053, 6.433245697403498, 5374.453811645509,
       0.6675424961095526},
      {0, 0.77, 4926.453811645513, 96.14667878447929, 7.738413838747228, 4926.453811645513,
       1.9727106374532828},
      {2877, 0.0, 5.320489880014939, 218.5579287155974, 3715.775680541992, 3715.775680541992,
       -0.9169793163046409},
      {2877, 0.3, 7.1544485126241915, 218.5474555741541, 5635.775680541997, 5635.775680541997,
       0.9169793163046113},
      {2877, 0.77, 6.448534396814493, 219.481059346779, 1603.7756805419804, 1603.7756805419804,
       0.2110652004949126},
      {4316, 0.0, 3194.4350158959533, 305.75164036678126, 663.9590263366699, 663.9590263366699,
       2.1444885522033816},
      {4316, 0.3, 3195.066407716563, 307.07973898574375, 1303.9590263366676, 1303.9590263366676,
       2.775880372812888},
      {4316, 0.77, 3191.1085038860174, 306.26170591053506, 3799.9590263366827, 3799.9590263366827,
       -1.1820234577327955},
      {8631, 0.0, 5767.028345187353, 409.29632889004347, 661.5976810455322, 661.5976810455322,
       -2.273900906397058},
      {8631, 0.3, 5771.407729961993, 409.86663474777856, 5781.597681045537, 5781.597681045537,
       2.1054838682429096},
      {8631, 0.77, 5769.896440000929, 408.0495795886672, 1429.5976810455159, 1429.5976810455159,
       0.5941939071792278}},
     12,
     {{0, 0.0, 1169.1048622131348, 451.41308960965654, 3840.0, 1169.1048622131348},
      {0, 0.41, 4113.104862213135, 448.6611166614364, 3840.0, 4113.104862213135},
      {1, 0.0, 6392.074966430664, 500.78333802661814, 5120.0, 6392.074966430664},
      {1, 0.41, 2296.0749664306645, 502.8553710145226, 5120.0, 2296.0749664306645},
      {2, 0.0, 3784.430694580078, 522.1493154279901, 1920.0, 3784.430694580078},
      {2, 0.41, 328.4306945800779, 521.6647958761738, 1920.0, 328.4306945800779},
      {3, 0.0, 2795.699119567871, 465.2725754257465, 2560.0, 2795.699119567871},
      {3, 0.41, 5099.699119567872, 466.21122208723364, 2560.0, 5099.699119567872},
      {4, 0.0, 3200.0, 189.9045078281512, 2983.021354675293, 2983.021354675293},
      {4, 0.41, 3200.0, 190.10503737636262, 3623.0213546752907, 3623.0213546752907},
      {5, 0.0, 1806.0232162475586, 234.81754233346328, 4480.0, 1806.0232162475586},
      {5, 0.41, 1166.0232162475609, 234.62956619370615, 4480.0, 1166.0232162475609}}},
    {1984u,
     0.5,
     272362444u,
     160,
     4311,
     {0, 2840, 3459, 4311},
     {13859332.09633255, 987537.4222106934, 2150.0, 17.0, 129242.0, 2154.23036965738, 9990.277281403542,
      1443928.0, 4652.626126587391, 2586992.0, 276388.1983947754, 4393.729824483395, 2154.976318359375,
      2323.0, 7941.0, 10871.0},
     {10.3465576171875, 92.54412078857422, 0.0, 1.0, 19.0, 0.9028499722480774, 1.748717188835144, 443.0,
      1.4762861728668213, 707.0, 54.40973663330078, 1.1145737171173096, 0.809814453125, 0.0, 3.0, 1.0},
     {5768.564453125, 406.6883239746094, 1.0, -1.0, 43.0, 0.3968968689441681, 2.9507768154144287, 373.0,
      0.8915607333183289, 493.0, 61.29216766357422, 1.0170589685440063, 0.171142578125, 2.0, 4.0, 0.0},
     341637.0,
     6,
     {4480.0, 566.0, 1.0, 1.0,  6.0,  0.19924503564834595, 0.34765625,    0.0,
      640.0,  503.0, 1.0, -1.0, 4.0,  0.6193450093269348,  0.8828125,     1.0,
      5760.0, 450.0, 1.0, 1.0,  6.0,  0.4499560296535492,  0.1865234375,  0.0,
      3200.0, 467.0, 1.0, -1.0, 4.0,  0.16180381178855896, 0.77294921875, 1.0,
      5760.0, 190.0, 0.0, 1.0,  10.0, 0.6275776624679565,  0.32861328125, 2.0,
      5120.0, 235.0, 1.0, -1.0, 10.0, 0.21699905395507812, 0.53857421875, 2.0},
     0,
     {{0}},
     0,
     {{0}}},
    {1984u,
     0.25,
     272362444u,
     160,
     2148,
     {0, 1407, 1704, 2148},
     {6907035.280408382, 491750.9837112427, 1071.0, 10.0, 64378.0, 1083.635543936527, 4989.147649526596,
      720560.0, 2336.172850549221, 1291498.0, 137498.7599849701, 2189.6986471414566, 1068.826904296875,
      1185.0, 3963.0, 5426.0},
     {10.3465576171875, 92.54412078857422, 0.0, 1.0, 19.0, 0.9223042130470276, 1.748717188835144, 443.0,
      1.4762861728668213, 707.0, 54.40973663330078, 1.1145737171173096, 0.809814453125, 0.0, 3.0, 1.0},
     {5770.16796875, 406.9068298339844, 1.0, -1.0, 43.0, 0.32740768790245056, 2.192225933074951, 310.0,
      0.9209485054016113, 539.0, 83.37962341308594, 0.9760216474533081, 0.3974609375, 2.0, 2.0, 4.0},
     170385.0,
     6,
     {4480.0, 566.0, 1.0, 1.0,  6.0,  0.19924503564834595, 0.34765625,    0.0,
      640.0,  503.0, 1.0, -1.0, 4.0,  0.6193450093269348,  0.8828125,     1.0,
      5760.0, 450.0, 1.0, 1.0,  6.0,  0.4499560296535492,  0.1865234375,  0.0,
      3200.0, 467.0, 1.0, -1.0, 4.0,  0.16180381178855896, 0.77294921875, 1.0,
      5760.0, 190.0, 0.0, 1.0,  10.0, 0.6275776624679565,  0.32861328125, 2.0,
      5120.0, 235.0, 1.0, -1.0, 10.0, 0.21699905395507812, 0.53857421875, 2.0},
     0,
     {{0}},
     0,
     {{0}}},
    {1984u,
     0.02,
     272362444u,
     160,
     480,
     {0, 321, 378, 480},
     {1535985.4578113556, 116679.24362945557, 240.0, 0.0, 15006.0, 242.603629535668, 1106.6167582273483,
      160941.0, 524.760473459959, 287180.0, 32270.996738433838, 488.99374532699585, 236.7265625, 261.0, 860.0,
      1141.0},
     {10.3465576171875, 92.54412078857422, 0.0, 1.0, 19.0, 0.08594886213541031, 1.748717188835144, 443.0,
      1.4762861728668213, 707.0, 54.40973663330078, 1.1145737171173096, 0.809814453125, 0.0, 3.0, 1.0},
     {5767.130859375, 409.2566833496094, 1.0, -1.0, 43.0, 0.7297483086585999, 2.2060842514038086, 412.0,
      0.7785066366195679, 532.0, 77.77693939208984, 0.8301559090614319, 0.538330078125, 2.0, 2.0, 7.0},
     38160.0,
     6,
     {4480.0, 566.0, 1.0, 1.0,  6.0,  0.19924503564834595, 0.34765625,    0.0,
      640.0,  503.0, 1.0, -1.0, 4.0,  0.6193450093269348,  0.8828125,     1.0,
      5760.0, 450.0, 1.0, 1.0,  6.0,  0.4499560296535492,  0.1865234375,  0.0,
      3200.0, 467.0, 1.0, -1.0, 4.0,  0.16180381178855896, 0.77294921875, 1.0,
      5760.0, 190.0, 0.0, 1.0,  10.0, 0.6275776624679565,  0.32861328125, 2.0,
      5120.0, 235.0, 1.0, -1.0, 10.0, 0.21699905395507812, 0.53857421875, 2.0},
     0,
     {{0}},
     0,
     {{0}}},
};

static const GoldenHull GOLDEN_HULLS[] = {
    {38,
     132,
     4,
     4,
     37.71999981254339,
     142.5249998047948,
     56.0,
     0.0,
     8287.0,
     {-1.7000000476837158, -0.5199999809265137, -4.0, 1.7000000476837158, 1.2599999904632568,
      3.5999999046325684},
     {0.33000001311302185, -0.1599999964237213, -4.0, 0.550000011920929, -0.1599999964237213, -4.0,
      0.30250000953674316, -0.1599999964237213, -4.0},
     {-1.2999999523162842, 1.2599999904632568, 3.5999999046325684, -1.3600000143051147, 0.30000001192092896,
      3.5999999046325684, -1.0800000429153442, 0.30000001192092896, 3.5999999046325684}},
    {36,
     162,
     4,
     3,
     45.89999909698963,
     377.799992993474,
     56.0,
     0.0,
     11183.0,
     {-3.549999952316284, -1.75, -9.0, 3.549999952316284, 2.1500000953674316, 9.199999809265137},
     {1.5, -1.350000023841858, -9.0, 1.25, 0.10000000149011612, -9.0, -1.25, 0.10000000149011612, -9.0},
     {-2.3499999046325684, 0.75, 9.199999809265137, -3.549999952316284, 0.75, 9.199999809265137,
      -3.549999952316284, -0.75, 9.199999809265137}},
    {36,
     90,
     3,
     3,
     -2.999999925494194,
     55.52000080049038,
     48.0,
     0.0,
     5624.0,
     {-1.7000000476837158, -0.41999998688697815, -1.600000023841858, 1.7000000476837158, 0.41999998688697815,
      0.949999988079071},
     {0.0, -0.05000000074505806, -1.600000023841858, 0.0, -0.05000000074505806, -1.600000023841858, 0.0,
      -0.05000000074505806, -1.600000023841858},
     {-1.100000023841858, 0.0, 0.8500000238418579, -1.100000023841858, 0.0, 0.8500000238418579,
      -1.100000023841858, 0.0, 0.8500000238418579}},
    {410,
     2088,
     31,
     31,
     5882.800001531839,
     26242.755781143904,
     716.0,
     398.0,
     1692675.0,
     {-40.0, -31.0, -98.0, 40.0, 41.0, 102.0},
     {1.7999999523162842, -5.0, -98.0, 3.0, -2.0, -98.0, 2.700000047683716, 1.5, -98.0},
     {-2.0, 1.399999976158142, 31.5, -5.0, 1.399999976158142, 31.5, -5.0, -1.399999976158142, 31.5}},
};

static const GoldenLights GOLDEN_LIGHTS = {76,
                                           {51.0, -7.105427357601002e-15, -464.7000000000001,
                                            985.2000000000003, 583.7999999999997, 266.24, 187.0,
                                            12.799999999999999},
                                           {39, 23, 14},
                                           {0.0, -11.338461538461539, -3.0, -84.0, 3.2, 4.0, 0.0, 0.0},
                                           {2.0, 0.0, -4.4, -7.5, 1.0, 7.0, 11.0, 0.0}};

static const double GOLDEN_LIGHT_TABLE[] = {
    -0.8,  0.0,  3.5,   1.9,  0.8,   0.0,   3.5, 1.9,   0.0,  -0.14, -4.1, 1.2, 0.0,  1.05, 0.7,
    0.9,   0.0,  -0.66, 0.6,  0.9,   -2.95, 0.0, 9.4,   2.8,  2.95,  0.0,  9.4, 2.8,  0.0,  -0.9,
    -9.15, 1.7,  0.0,   2.35, -3.6,  1.2,   0.0, -1.95, 2.0,  1.2,   -1.1, 0.0, 0.95, 1.0,  1.1,
    0.0,   0.95, 1.0,   0.0,  -0.05, -1.65, 0.8, 0.0,   0.55, 0.0,   0.6,  0.0, 0.0,  0.0,  0.0};
static const double GOLDEN_TRAIL_TABLE[] = {0.0, 3.5, 0.8, 0.3, 0.0, 9.4, 2.95, 0.5, 0.0, 0.95, 1.1, 0.2};

/* Float32 data read back into doubles: allow the last bit. */
static double tol(double v) {
    return fabs(v) * 2e-7 + 1e-9;
}

static void build(const GoldenCase *g) {
    swc_city_build(&city, g->seed);
    swc_air_pack_craft(&city, g->density, swc_air_layer_seed(g->seed), &craft);
    swc_air_plan_heroes(&city, swc_air_layer_seed(g->seed), &heroes);
}

static void test_the_craft_pack_matches_the_web_version(void) {
    for (size_t c = 0; c < sizeof(GOLDEN) / sizeof(GOLDEN[0]); c++) {
        const GoldenCase *g = &GOLDEN[c];
        build(g);
        CHECK_EQ_INT((long long)swc_air_layer_seed(g->seed), (long long)g->layer_seed);
        CHECK_EQ_INT(craft.count, g->count);
        for (int s = 0; s < 4; s++)
            CHECK_EQ_INT(craft.shape_start[s], g->shape_start[s]);
        double sums[16] = {0}, lane_sum = 0.0;
        for (int i = 0; i < craft.count; i++) {
            for (int k = 0; k < 16; k++)
                sums[k] += craft.data[i * 16 + k];
            lane_sum += craft.lane_of[i];
        }
        for (int k = 0; k < 16; k++) {
            CHECK_NEAR(sums[k], g->sums[k], tol(g->sums[k]) * 4);
            CHECK_NEAR(craft.data[k], g->first[k], tol(g->first[k]));
            CHECK_NEAR(craft.data[(craft.count - 1) * 16 + k], g->last[k], tol(g->last[k]));
        }
        CHECK_NEAR(lane_sum, g->lane_sum, 0.5);
    }
}

static void test_the_craft_motion_matches_the_web_version(void) {
    int checked = 0;
    for (size_t c = 0; c < sizeof(GOLDEN) / sizeof(GOLDEN[0]); c++) {
        const GoldenCase *g = &GOLDEN[c];
        if (g->state_count == 0)
            continue;
        build(g);
        for (int k = 0; k < g->state_count; k++) {
            const GoldenState *e = &g->states[k];
            SwcCraftState s;
            swc_air_craft_state(craft.data, e->index, e->loop, &s);
            CHECK_NEAR(s.x, e->x, 1e-6);
            CHECK_NEAR(s.y, e->y, 1e-6);
            CHECK_NEAR(s.z, e->z, 1e-6);
            CHECK_NEAR(s.along, e->along, 1e-6);
            CHECK_NEAR(s.lateral, e->lateral, 1e-6);
            checked++;
        }
    }
    CHECK(checked >= 36);
}

static void test_the_hero_routes_match_the_web_version(void) {
    for (size_t c = 0; c < sizeof(GOLDEN) / sizeof(GOLDEN[0]); c++) {
        const GoldenCase *g = &GOLDEN[c];
        build(g);
        CHECK_EQ_INT(heroes.count, g->hero_count);
        for (int k = 0; k < g->hero_count * SWC_HERO_STRIDE; k++)
            CHECK_NEAR(heroes.data[k], g->hero[k], tol(g->hero[k]));
        for (int k = 0; k < g->hero_state_count; k++) {
            const GoldenHeroState *e = &g->hero_states[k];
            SwcHeroState s;
            swc_air_hero_state(heroes.data, e->index, e->loop, &s);
            CHECK_NEAR(s.x, e->x, 1e-6);
            CHECK_NEAR(s.y, e->y, 1e-6);
            CHECK_NEAR(s.z, e->z, 1e-6);
            CHECK_NEAR(s.along, e->along, 1e-6);
        }
    }
}

static void build_hull(int which) {
    if (which == 3)
        swc_air_build_hero_hull(&hull);
    else
        swc_air_build_craft_hull(which, &hull);
}

static void test_the_hulls_match_the_web_version(void) {
    for (int w = 0; w < 4; w++) {
        const GoldenHull *g = &GOLDEN_HULLS[w];
        build_hull(w);
        CHECK_EQ_INT(hull.vertex_count, g->vertices);
        CHECK_EQ_INT(hull.index_count, g->indices);
        CHECK_EQ_INT(hull.piece_count, g->pieces);
        int closed = 0;
        for (int p = 0; p < hull.piece_count; p++)
            closed += hull.piece_closed[p] ? 1 : 0;
        CHECK_EQ_INT(closed, g->closed);
        double pos = 0.0, abs_sum = 0.0, part = 0.0, type = 0.0, idx = 0.0;
        double bb[6] = {1e9, 1e9, 1e9, -1e9, -1e9, -1e9};
        for (int v = 0; v < hull.vertex_count; v++) {
            for (int k = 0; k < 3; k++) {
                double x = hull.position[v * 3 + k];
                pos += x;
                abs_sum += fabs(x);
                if (x < bb[k])
                    bb[k] = x;
                if (x > bb[k + 3])
                    bb[k + 3] = x;
            }
            part += hull.part[v];
            type += hull.type[v];
        }
        for (int i = 0; i < hull.index_count; i++)
            idx += (double)hull.index[i] * (double)((i % 7) + 1);
        CHECK_NEAR(pos, g->pos_sum, 2e-4);
        CHECK_NEAR(abs_sum, g->abs_sum, 2e-4);
        CHECK_NEAR(part, g->part_sum, 1e-9);
        CHECK_NEAR(type, g->type_sum, 1e-9);
        CHECK_NEAR(idx, g->idx_sum, 1e-9);
        for (int k = 0; k < 6; k++)
            CHECK_NEAR(bb[k], g->bb[k], 1e-5);
        for (int k = 0; k < 9; k++) {
            CHECK_NEAR(hull.position[k], g->first_pos[k], 1e-5);
            CHECK_NEAR(hull.position[hull.vertex_count * 3 - 9 + k], g->last_pos[k], 1e-5);
        }
    }
}

static void test_the_light_tables_and_hero_lights_match_the_web_version(void) {
    const float *lt = swc_air_craft_light_table();
    for (int k = 0; k < SWC_CRAFT_LIGHT_FLOATS; k++)
        CHECK_NEAR(lt[k], GOLDEN_LIGHT_TABLE[k], tol(GOLDEN_LIGHT_TABLE[k]));
    const float *tt = swc_air_craft_trail_table();
    for (int k = 0; k < SWC_CRAFT_TRAIL_FLOATS; k++)
        CHECK_NEAR(tt[k], GOLDEN_TRAIL_TABLE[k], tol(GOLDEN_TRAIL_TABLE[k]));

    int n = swc_air_hero_lights(lights);
    CHECK_EQ_INT(n, GOLDEN_LIGHTS.count);
    double sums[8] = {0};
    int by_type[3] = {0, 0, 0};
    for (int i = 0; i < n; i++) {
        const SwcHeroLight *l = &lights[i];
        double v[8] = {(double)l->type, l->x, l->y, l->z, l->size, l->color, (double)l->mode, l->phase};
        for (int k = 0; k < 8; k++)
            sums[k] += v[k];
        by_type[l->type]++;
    }
    for (int k = 0; k < 8; k++)
        CHECK_NEAR(sums[k], GOLDEN_LIGHTS.sums[k], 1e-3);
    for (int t = 0; t < 3; t++)
        CHECK_EQ_INT(by_type[t], GOLDEN_LIGHTS.by_type[t]);
    const SwcHeroLight *f = &lights[0], *z = &lights[n - 1];
    double fv[8] = {(double)f->type, f->x, f->y, f->z, f->size, f->color, (double)f->mode, f->phase};
    double zv[8] = {(double)z->type, z->x, z->y, z->z, z->size, z->color, (double)z->mode, z->phase};
    for (int k = 0; k < 8; k++) {
        CHECK_NEAR(fv[k], GOLDEN_LIGHTS.first[k], 1e-5);
        CHECK_NEAR(zv[k], GOLDEN_LIGHTS.last[k], 1e-5);
    }
}

/* ---- properties ---- */

static void test_the_pack_is_deterministic_and_the_seed_matters(void) {
    swc_city_build(&city, 1984);
    swc_air_pack_craft(&city, 1.0, swc_air_layer_seed(1984), &craft);
    swc_air_pack_craft(&city, 1.0, swc_air_layer_seed(1984), &craft2);
    CHECK_EQ_INT(craft.count, craft2.count);
    CHECK(memcmp(craft.data, craft2.data, (size_t)craft.count * 16 * sizeof(float)) == 0);
    swc_air_pack_craft(&city, 1.0, swc_air_layer_seed(1985), &craft2);
    CHECK(memcmp(craft.data, craft2.data, (size_t)craft.count * 16 * sizeof(float)) != 0);
}

static void test_the_sky_holds_thousands_of_craft_and_density_thins_them(void) {
    swc_city_build(&city, 1984);
    swc_air_pack_craft(&city, 1.0, swc_air_layer_seed(1984), &craft);
    CHECK(craft.count >= 8000 && craft.count <= 12000);
    CHECK(craft.count <= SWC_CRAFT_MAX);
    int half_count, quarter_count;
    swc_air_pack_craft(&city, 0.5, swc_air_layer_seed(1984), &craft2);
    half_count = craft2.count;
    swc_air_pack_craft(&city, 0.25, swc_air_layer_seed(1984), &craft2);
    quarter_count = craft2.count;
    CHECK(half_count < craft.count * 0.55 && half_count > craft.count * 0.45);
    CHECK(quarter_count < half_count * 0.55 && quarter_count > half_count * 0.45);
    swc_air_pack_craft(&city, 0.0, swc_air_layer_seed(1984), &craft2); /* the floor is three per lane */
    CHECK(craft2.count >= 3 * 160);
}

static void test_the_craft_are_sorted_by_shape_with_every_shape_present(void) {
    swc_city_build(&city, 1984);
    swc_air_pack_craft(&city, 1.0, swc_air_layer_seed(1984), &craft);
    CHECK_EQ_INT(craft.shape_start[0], 0);
    CHECK_EQ_INT(craft.shape_start[3], craft.count);
    for (int s = 0; s < 3; s++) {
        CHECK(craft.shape_start[s + 1] > craft.shape_start[s]);
        for (int i = craft.shape_start[s]; i < craft.shape_start[s + 1]; i++)
            CHECK_EQ_INT((int)craft.data[i * 16 + SWC_CRAFT_SHAPE], s);
    }
    /* mostly cars, then drones, then haulers */
    CHECK(craft.shape_start[1] > craft.count / 2);
}

static void test_lanes_run_at_25_to_70_metres_a_second_and_higher_lanes_are_faster(void) {
    int lo = (int)ceil(25.0 / LAP_SPEED), hi = (int)floor(70.0 / LAP_SPEED);
    for (int slot = 0; slot < 4; slot++)
        for (int k = 0; k <= 4; k++) {
            int laps = swc_air_laps_for_lane(slot, k / 4.0);
            CHECK(laps >= lo && laps <= hi);
        }
    CHECK(swc_air_laps_for_lane(3, 0.5) > swc_air_laps_for_lane(0, 0.5));
    /* spacing: about 86 m low down to 158 m high up, never fewer than three craft */
    CHECK_EQ_INT(swc_air_craft_in_lane(0, 0.0, 1.0), (int)floor(SWC_TILE / 86.0 + 0.5));
    CHECK_EQ_INT(swc_air_craft_in_lane(3, 1.0, 1.0), (int)floor(SWC_TILE / 158.0 + 0.5));
    CHECK_EQ_INT(swc_air_craft_in_lane(3, 1.0, 0.001), 3);
}

static const struct {
    double half_width, half_height, half_length, lateral;
} SHAPE_BOX[3] = {{1.75, 1.3, 4.1, 6.0}, {3.6, 2.3, 9.4, 4.5}, {1.7, 0.7, 1.7, 6.0}};

static void test_offsets_sway_and_hull_stay_inside_the_lane_tube(void) {
    swc_city_build(&city, 1984);
    swc_air_pack_craft(&city, 1.0, swc_air_layer_seed(1984), &craft);
    int sky[SWC_MAX_LANES], sky_count = 0;
    for (int i = 0; i < city.lane_count; i++)
        if (city.lanes[i].kind == SWC_LANE_SKY)
            sky[sky_count++] = i;
    CHECK_EQ_INT(sky_count, 160);
    double tube_half_width = SWC_AVENUE.corridor_half - SWC_SKY_LANE_OFFSET;
    for (int i = 0; i < craft.count; i++) {
        const float *r = &craft.data[i * 16];
        const SwcLane *lane = &city.lanes[sky[craft.lane_of[i]]];
        int s = (int)r[SWC_CRAFT_SHAPE];
        double scale = r[SWC_CRAFT_SCALE];
        double side = fabs(swc_wrap_delta(r[SWC_CRAFT_FIXED], lane->fixed, SWC_TILE));
        double lateral = side + r[SWC_CRAFT_WEAVE_AMP];
        CHECK(lateral <= SWC_SKY_LANE_MAX_WEAVE + 1e-4);
        CHECK(lateral <= SHAPE_BOX[s].lateral + 1e-4);
        CHECK(lateral + SHAPE_BOX[s].half_width * scale <= tube_half_width + 1e-4);
        CHECK(lateral + SHAPE_BOX[s].half_width * scale < SWC_SKY_LANE_OFFSET); /* off the centre line */
        double vertical = fabs(r[SWC_CRAFT_Y] - lane->y) + r[SWC_CRAFT_BOB_AMP];
        CHECK(vertical + SHAPE_BOX[s].half_height * scale <= 10.0 + 1e-4);
        CHECK_EQ_INT((int)r[SWC_CRAFT_AXIS], lane->axis);
        CHECK_EQ_INT((int)r[SWC_CRAFT_DIR], lane->dir);
    }
}

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}

static void test_craft_in_a_lane_share_a_speed_and_keep_a_gap_longer_than_the_longest_hull(void) {
    swc_city_build(&city, 1984);
    swc_air_pack_craft(&city, 1.0, swc_air_layer_seed(1984), &craft);
    static double phases[SWC_CRAFT_MAX];
    double smallest = 1e30;
    int lanes_seen = 0;
    for (int lane = 0; lane < 160; lane++) {
        int n = 0;
        double laps = -1.0;
        for (int i = 0; i < craft.count; i++) {
            if (craft.lane_of[i] != lane)
                continue;
            if (laps < 0.0)
                laps = craft.data[i * 16 + SWC_CRAFT_LAPS];
            CHECK_NEAR(craft.data[i * 16 + SWC_CRAFT_LAPS], laps, 0.0);
            phases[n++] = craft.data[i * 16 + SWC_CRAFT_PHASE];
        }
        CHECK(n > 0);
        lanes_seen++;
        qsort(phases, (size_t)n, sizeof(double), cmp_double);
        double spacing = SWC_TILE / n;
        CHECK(spacing >= 70.0 && spacing <= 160.0);
        for (int k = 0; k < n; k++) {
            double next = k + 1 < n ? phases[k + 1] : phases[0] + 1.0;
            double gap = (next - phases[k]) * SWC_TILE;
            if (gap < smallest)
                smallest = gap;
            CHECK(gap >= spacing * (1.0 - 2.0 * 0.3) - 0.01);
        }
    }
    CHECK_EQ_INT(lanes_seen, 160);
    CHECK(smallest >
          2.0 * 9.4 + 5.0); /* the longest hull, a hauler, is 18.8 m (its anchor is not its middle) */
}

static void test_the_motion_is_periodic_and_continuous_across_the_loop_seam(void) {
    swc_city_build(&city, 1984);
    swc_air_pack_craft(&city, 1.0, swc_air_layer_seed(1984), &craft);
    double dt = 1.0 / 120.0, eps = dt / SWC_LOOP_SECONDS;
    for (int i = 0; i < craft.count; i += 3) {
        SwcCraftState a, b;
        swc_air_craft_state(craft.data, i, 1.0 - eps, &a);
        swc_air_craft_state(craft.data, i, eps, &b);
        double speed = craft.data[i * 16 + SWC_CRAFT_LAPS] * LAP_SPEED;
        double dx = swc_wrap_delta(b.x, a.x, SWC_TILE), dz = swc_wrap_delta(b.z, a.z, SWC_TILE);
        double moved = sqrt(dx * dx + (b.y - a.y) * (b.y - a.y) + dz * dz);
        CHECK(moved > speed * 2.0 * dt * 0.9);
        CHECK(moved < speed * 2.0 * dt * 1.1 + 0.05);
        swc_air_craft_state(craft.data, i, 0.0, &a);
        swc_air_craft_state(craft.data, i, 1.0, &b);
        CHECK(fabs(swc_wrap_delta(b.x, a.x, SWC_TILE)) < 1e-6);
        CHECK(fabs(b.y - a.y) < 1e-6);
        CHECK(fabs(swc_wrap_delta(b.z, a.z, SWC_TILE)) < 1e-6);
    }
}

static void test_each_craft_moves_the_way_its_lane_points(void) {
    swc_city_build(&city, 1984);
    swc_air_pack_craft(&city, 1.0, swc_air_layer_seed(1984), &craft);
    for (int i = 0; i < craft.count; i += 11) {
        SwcCraftState a, b;
        swc_air_craft_state(craft.data, i, 0.25, &a);
        swc_air_craft_state(craft.data, i, 0.25 + 1.0 / SWC_LOOP_SECONDS, &b);
        double d = swc_wrap_delta(b.along, a.along, SWC_TILE);
        CHECK_EQ_INT(d > 0.0 ? 1 : -1, (int)craft.data[i * 16 + SWC_CRAFT_DIR]);
    }
}

static void test_the_hero_plan_has_two_of_each_type_and_is_deterministic(void) {
    swc_city_build(&city, 1984);
    swc_air_plan_heroes(&city, swc_air_layer_seed(1984), &heroes);
    SwcHeroPack again;
    swc_air_plan_heroes(&city, swc_air_layer_seed(1984), &again);
    CHECK_EQ_INT(heroes.count, 6);
    CHECK_EQ_INT(again.count, heroes.count);
    CHECK(memcmp(heroes.data, again.data, sizeof(float) * SWC_HERO_STRIDE * (size_t)heroes.count) == 0);
    int types[3] = {0, 0, 0};
    for (int i = 0; i < heroes.count; i++) {
        const float *h = &heroes.data[i * SWC_HERO_STRIDE];
        types[(int)h[SWC_HERO_TYPE]]++;
        CHECK_EQ_INT((int)h[SWC_HERO_LAPS], swc_air_hero_limits((int)h[SWC_HERO_TYPE])->laps);
        CHECK_NEAR(fmod(h[SWC_HERO_FIXED], SWC_CELL * 4.0), 0.0, 0.0); /* on an avenue */
    }
    CHECK(types[0] == 2 && types[1] == 2 && types[2] == 2);
}

static void test_the_wide_ships_share_an_axis_on_different_avenues(void) {
    swc_city_build(&city, 1984);
    swc_air_plan_heroes(&city, swc_air_layer_seed(1984), &heroes);
    int axis = -1;
    double fixed[8];
    int wide = 0;
    for (int i = 0; i < heroes.count; i++) {
        const float *h = &heroes.data[i * SWC_HERO_STRIDE];
        if ((int)h[SWC_HERO_TYPE] == SWC_HERO_PATROL)
            continue;
        if (axis < 0)
            axis = (int)h[SWC_HERO_AXIS];
        CHECK_EQ_INT((int)h[SWC_HERO_AXIS], axis);
        for (int k = 0; k < wide; k++)
            CHECK(fixed[k] != h[SWC_HERO_FIXED]);
        fixed[wide++] = h[SWC_HERO_FIXED];
    }
    CHECK_EQ_INT(wide, 4);
}

static void test_the_wide_ships_fly_above_every_sky_lane_and_roof_beside_them(void) {
    static const double levels[8] = {90, 170, 260, 360, 130, 215, 310, 410};
    swc_city_build(&city, 1984);
    swc_air_plan_heroes(&city, swc_air_layer_seed(1984), &heroes);
    for (int i = 0; i < heroes.count; i++) {
        const float *h = &heroes.data[i * SWC_HERO_STRIDE];
        int type = (int)h[SWC_HERO_TYPE];
        if (type == SWC_HERO_PATROL)
            continue;
        const SwcHeroLimits *lim = swc_air_hero_limits(type);
        double underside = h[SWC_HERO_Y] - lim->below - lim->bob_amp;
        CHECK(underside >= 432.0);
        for (int k = 0; k < 8; k++)
            CHECK(underside > levels[k] + 10.0);
        /* the roofs beside the avenue: the plan's own boxes, swept under the ship's full width */
        int axis = (int)h[SWC_HERO_AXIS];
        int line = (int)floor(h[SWC_HERO_FIXED] / SWC_CELL + 0.5);
        double tallest = 0.0;
        for (int along = 0; along < SWC_CELLS; along++)
            for (int side = -1; side <= 0; side++) {
                int cx = axis == 0 ? along : line + side, cz = axis == 0 ? line + side : along;
                double hh = city.cells[swc_cell_index(cx, cz)].height * 4.0;
                if (hh > tallest)
                    tallest = hh;
            }
        CHECK(underside >= tallest + 28.0);
        for (int k = 0; k < 40; k++) {
            SwcHeroState s;
            swc_air_hero_state(heroes.data, i, k / 40.0, &s);
            CHECK(fabs(s.y - h[SWC_HERO_Y]) <= lim->bob_amp + 1e-9);
        }
    }
}

static void test_the_patrol_craft_thread_the_canyon_between_lane_levels(void) {
    static const double levels[8] = {90, 170, 260, 360, 130, 215, 310, 410};
    swc_city_build(&city, 1984);
    swc_air_plan_heroes(&city, swc_air_layer_seed(1984), &heroes);
    const SwcHeroLimits *lim = swc_air_hero_limits(SWC_HERO_PATROL);
    CHECK(lim->half_width < SWC_AVENUE.corridor_half);
    double ys[2];
    int found = 0;
    for (int i = 0; i < heroes.count; i++) {
        const float *h = &heroes.data[i * SWC_HERO_STRIDE];
        if ((int)h[SWC_HERO_TYPE] != SWC_HERO_PATROL)
            continue;
        double y = h[SWC_HERO_Y];
        ys[found++] = y;
        for (int k = 0; k < 8; k++) {
            double level = levels[k];
            double clear = level > y ? level - 10.0 - (y + lim->above + lim->bob_amp)
                                     : y - lim->below - lim->bob_amp - (level + 10.0);
            CHECK(clear > 2.0);
        }
    }
    CHECK_EQ_INT(found, 2);
    CHECK(fabs(ys[0] - ys[1]) > lim->above + lim->below + 2.0 * lim->bob_amp);
}

static void test_the_hero_ships_move_periodically_and_other_cities_plan_too(void) {
    static const uint32_t seeds[4] = {1984, 1, 7, 99};
    for (int c = 0; c < 4; c++) {
        swc_city_build(&city, seeds[c]);
        swc_air_plan_heroes(&city, swc_air_layer_seed(seeds[c]), &heroes);
        CHECK_EQ_INT(heroes.count, 6);
        for (int i = 0; i < heroes.count; i++) {
            SwcHeroState a, b;
            swc_air_hero_state(heroes.data, i, 0.0, &a);
            swc_air_hero_state(heroes.data, i, 1.0, &b);
            CHECK(fabs(swc_wrap_delta(b.x, a.x, SWC_TILE)) + fabs(b.y - a.y) +
                      fabs(swc_wrap_delta(b.z, a.z, SWC_TILE)) <
                  1e-6);
        }
    }
}

/* ---- hulls ---- */

/* Six times the signed volume a run of triangles encloses: positive when they are wound outwards. */
static double signed_volume6(const SwcHull *h, int start, int len) {
    double volume = 0.0;
    for (int t = start; t < start + len; t += 3) {
        const float *a = &h->position[h->index[t] * 3], *b = &h->position[h->index[t + 1] * 3],
                    *c = &h->position[h->index[t + 2] * 3];
        volume += (double)a[0] * ((double)b[1] * c[2] - (double)b[2] * c[1]) +
                  (double)a[1] * ((double)b[2] * c[0] - (double)b[0] * c[2]) +
                  (double)a[2] * ((double)b[0] * c[1] - (double)b[1] * c[0]);
    }
    return volume;
}

/* Every piece (the open ones included) is wound outwards and carries one part id, so a flat varying cannot
 * pick up a neighbour's. */
static void check_piece(const SwcHull *h, int p) {
    int start = h->piece_start[p], len = h->piece_length[p];
    CHECK(len > 0);
    CHECK(signed_volume6(h, start, len) > 0.0);
    float part = h->part[h->index[start]];
    for (int t = start; t < start + len; t++)
        CHECK(h->part[h->index[t]] == part);
}

static void test_every_hull_piece_is_wound_outwards_with_one_part_id(void) {
    int checked = 0;
    for (int w = 0; w < 4; w++) {
        build_hull(w);
        for (int p = 0; p < hull.piece_count; p++) {
            check_piece(&hull, p);
            checked++;
        }
        int covered = 0;
        for (int p = 0; p < hull.piece_count; p++)
            covered += hull.piece_length[p];
        CHECK_EQ_INT(covered, hull.index_count);
        for (int i = 0; i < hull.index_count; i++)
            CHECK(hull.index[i] < hull.vertex_count);
        CHECK(hull.index_count % 3 == 0);
    }
    CHECK(checked >= 40);
}

static void test_the_craft_hulls_stay_inside_the_limits_the_packer_assumes(void) {
    for (int s = 0; s < 3; s++) {
        build_hull(s);
        for (int v = 0; v < hull.vertex_count; v++) {
            CHECK(fabs(hull.position[v * 3]) <= SHAPE_BOX[s].half_width + 1e-5);
            CHECK(fabs(hull.position[v * 3 + 1]) <= SHAPE_BOX[s].half_height + 1e-5);
            CHECK(fabs(hull.position[v * 3 + 2]) <= SHAPE_BOX[s].half_length + 1e-5);
        }
        CHECK(hull.index_count / 3 <= 60); /* cheap: several thousand of each are drawn, twice */
    }
}

static void test_the_hero_hulls_fit_the_extents_the_route_planner_clears(void) {
    build_hull(3);
    int seen[3] = {0, 0, 0};
    for (int v = 0; v < hull.vertex_count; v++) {
        int t = (int)hull.type[v];
        CHECK(t >= 0 && t < 3);
        seen[t]++;
        const SwcHeroLimits *lim = swc_air_hero_limits(t);
        CHECK(fabs(hull.position[v * 3]) <= lim->half_width + 1e-5);
        CHECK(fabs(hull.position[v * 3 + 2]) <= lim->half_length + 1e-5);
        CHECK(hull.position[v * 3 + 1] >= -lim->below - 1e-5);
        CHECK(hull.position[v * 3 + 1] <= lim->above + 1e-5);
    }
    CHECK(seen[0] > 0 && seen[1] > 0 && seen[2] > 0);
}

static void test_the_hero_lights_have_sane_kinds_and_only_the_patrol_craft_has_a_searchlight(void) {
    int n = swc_air_hero_lights(lights);
    int cones = 0, pools = 0, lamps = 0;
    for (int i = 0; i < n; i++) {
        const SwcHeroLight *l = &lights[i];
        CHECK(l->type >= 0 && l->type < 3);
        CHECK(l->size > 0.0f);
        CHECK(l->color >= 0.0f && l->color < 8.0f);
        CHECK(l->mode == SWC_GLOW_STEADY || l->mode == SWC_GLOW_STROBE || l->mode == SWC_GLOW_PULSE ||
              l->mode == SWC_GLOW_POLICE_A || l->mode == SWC_GLOW_POLICE_B || l->mode == SWC_GLOW_ENGINE ||
              l->mode == SWC_GLOW_SEARCHLAMP || l->mode == SWC_GLOW_HEAD || l->mode == SWC_GLOW_CONE ||
              l->mode == SWC_GLOW_POOL || l->mode == SWC_GLOW_PLUME);
        if (l->mode == SWC_GLOW_CONE) {
            cones++;
            CHECK_EQ_INT(l->type, SWC_HERO_PATROL);
        }
        if (l->mode == SWC_GLOW_POOL) {
            pools++;
            CHECK_EQ_INT(l->type, SWC_HERO_PATROL);
        }
        if (l->mode == SWC_GLOW_SEARCHLAMP) {
            lamps++;
            CHECK_EQ_INT(l->type, SWC_HERO_PATROL);
        }
    }
    CHECK(cones == 1 && pools == 1 && lamps == 1);
    CHECK(n <= SWC_HERO_LIGHT_MAX);
}

int main(void) {
    RUN(test_the_craft_pack_matches_the_web_version);
    RUN(test_the_craft_motion_matches_the_web_version);
    RUN(test_the_hero_routes_match_the_web_version);
    RUN(test_the_hulls_match_the_web_version);
    RUN(test_the_light_tables_and_hero_lights_match_the_web_version);
    RUN(test_the_pack_is_deterministic_and_the_seed_matters);
    RUN(test_the_sky_holds_thousands_of_craft_and_density_thins_them);
    RUN(test_the_craft_are_sorted_by_shape_with_every_shape_present);
    RUN(test_lanes_run_at_25_to_70_metres_a_second_and_higher_lanes_are_faster);
    RUN(test_offsets_sway_and_hull_stay_inside_the_lane_tube);
    RUN(test_craft_in_a_lane_share_a_speed_and_keep_a_gap_longer_than_the_longest_hull);
    RUN(test_the_motion_is_periodic_and_continuous_across_the_loop_seam);
    RUN(test_each_craft_moves_the_way_its_lane_points);
    RUN(test_the_hero_plan_has_two_of_each_type_and_is_deterministic);
    RUN(test_the_wide_ships_share_an_axis_on_different_avenues);
    RUN(test_the_wide_ships_fly_above_every_sky_lane_and_roof_beside_them);
    RUN(test_the_patrol_craft_thread_the_canyon_between_lane_levels);
    RUN(test_the_hero_ships_move_periodically_and_other_cities_plan_too);
    RUN(test_every_hull_piece_is_wound_outwards_with_one_part_id);
    RUN(test_the_craft_hulls_stay_inside_the_limits_the_packer_assumes);
    RUN(test_the_hero_hulls_fit_the_extents_the_route_planner_clears);
    RUN(test_the_hero_lights_have_sane_kinds_and_only_the_patrol_craft_has_a_searchlight);
    return TEST_SUMMARY();
}
