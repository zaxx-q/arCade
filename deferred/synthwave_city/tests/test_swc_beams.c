/* The beams layer's instance data. The numbers marked "web version" were produced by the original TypeScript
 * packer (src/layers/beams/pack.ts) for the same seeds and densities, so they pin the port to it: which sites
 * are lit, every random draw in order, and the closed-form direction of a beam at a given moment. */
#include <math.h>
#include <string.h>

#include "arcade_test.h"
#include "swc_beams.h"

static SwcCity city;
static SwcCity trimmed;
static SwcPackedBeams beams;
static SwcPackedBeams beams2;
static bool lit[SWC_MAX_SITES];

#define PI 3.14159265358979323846
#define FLOATS_PER_BEAM 27 /* origin 3, shape 4, look 4, tilt 4, azim 4, fan 4, roll 4 */

typedef struct GoldenSet {
    int count;
    double sums[7];                /* sum of every float of origin, shape, look, tilt, azim, fan, roll */
    double first[FLOATS_PER_BEAM]; /* the first beam's floats in that order */
    double last[FLOATS_PER_BEAM];
} GoldenSet;

typedef struct Golden {
    uint32_t seed;
    double density;
    uint32_t layer_seed;
    int lit_count;
    double lit_sum; /* sum of (site index + 1) over the lit sites */
    int used[3];    /* emitters lit: searchlight, pillar, laser */
    GoldenSet sky, pillars;
} Golden;

typedef struct GoldenDir {
    uint32_t seed;
    int set; /* 0 sky, 1 pillars */
    int index;
    double loop;
    double dir[3];
} GoldenDir;

static const Golden GOLDEN[] = {
    {1984u,
     1.0,
     607307797u,
     160,
     271313.0,
     {98, 56, 6},
     {140,
      {902212.597869873, 200317.37942692637, 1047.963269271655, 18067.914583280683, 10325.868856623769,
       9581.588869273663, 90.38368947803974},
      {4355.16015625,
       353.84002685546875,
       84.0,
       1838.6444091796875,
       0.16706186532974243,
       0.16706186532974243,
       2.0,
       2.0,
       0.3048999011516571,
       0.8775472044944763,
       -0.5,
       0.5337927341461182,
       0.10736444592475891,
       101.0,
       1.5096867084503174,
       2.061833143234253,
       -24.0,
       0.0,
       0.0,
       0.7172936797142029,
       0.3028102219104767,
       194.0,
       2.6429426670074463,
       1.4350614547729492,
       0.0,
       0.0,
       0.125},
      {3132.175537109375,
       0.5,
       6323.896484375,
       1002.5482788085938,
       1.444234013557434,
       39.18382263183594,
       0.0,
       7.0,
       0.3988910913467407,
       1.0507663488388062,
       0.0,
       0.6201812624931335,
       0.13836276531219482,
       118.0,
       5.431521415710449,
       0.30971473455429077,
       -56.0,
       0.2199999988079071,
       122.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.30971473455429077,
       1.0}},
     {56,
      {385606.68829345703, 116695.42226219177, 153.0743797365576, 0.0, 0.0, 0.0, 56.0},
      {5204.0,
       1025.4730224609375,
       84.0,
       2600.0,
       9.247846603393555,
       9.247846603393555,
       1.0,
       1.0,
       0.6511305570602417,
       1.2999999523162842,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       1.0},
      {84.0,
       0.0,
       3284.0,
       1924.4447021484375,
       5.376313209533691,
       5.376313209533691,
       1.0,
       2.0,
       0.7105469703674316,
       0.6499999761581421,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       1.0}}},
    {1u,
     1.0,
     2213883995u,
     151,
     243864.0,
     {92, 51, 8},
     {156,
      {1068094.754333496, 223744.61120057106, 1118.5075640100986, 22179.726536940783, 14471.986779694445,
       15512.961982905865, 532.7411110298708},
      {1196.0,
       146.27772521972656,
       39.19306564331055,
       1229.1981201171875,
       1.4053473472595215,
       43.47257995605469,
       0.0,
       1.0,
       0.6818553805351257,
       0.7838761210441589,
       0.0,
       0.7455212473869324,
       0.26189133524894714,
       122.0,
       1.4857609272003174,
       5.906428813934326,
       0.0,
       0.6139592528343201,
       110.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       5.161832809448242,
       1.0},
      {3772.243896484375,
       0.5,
       3607.75439453125,
       1258.0897216796875,
       1.4304200410842896,
       40.09354782104492,
       0.0,
       7.0,
       0.9572836756706238,
       1.0406190156936646,
       0.0,
       0.7317953705787659,
       0.14081092178821564,
       144.0,
       6.379602909088135,
       0.5645678043365479,
       -47.0,
       0.2199999988079071,
       162.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.5645678043365479,
       1.0}},
     {51,
      {357944.00506591797, 105950.25120830536, 146.3025848735124, 0.0, 0.0, 0.0, 51.0},
      {1040.0,
       764.1390380859375,
       400.0,
       2600.0,
       9.186216354370117,
       9.186216354370117,
       1.0,
       0.0,
       0.911577582359314,
       1.2999999523162842,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       1.0},
      {2960.0,
       0.0,
       1520.0,
       1618.5123291015625,
       5.203317642211914,
       5.203317642211914,
       1.0,
       1.0,
       0.12478490173816681,
       0.6499999761581421,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       1.0}}},
    {42u,
     1.0,
     1551129271u,
     142,
     203398.0,
     {82, 55, 5},
     {122,
      {778450.7053756714, 173606.27952200174, 830.5011579643469, 17463.375947639346, 11872.185350648127,
       10214.652138411999, -189.23416336718947},
      {3113.52734375,
       83.71395874023438,
       1152.20458984375,
       841.7068481445312,
       1.6344549655914307,
       25.763778686523438,
       0.0,
       1.0,
       0.8301387429237366,
       0.7501122355461121,
       0.0,
       0.6999605298042297,
       0.17315812408924103,
       120.0,
       1.178465723991394,
       2.9903738498687744,
       35.0,
       0.10000000149011612,
       83.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       3.6435048580169678,
       1.0},
      {6142.7119140625,
       0.5,
       5209.01318359375,
       923.2647094726562,
       2.0152170658111572,
       30.29815101623535,
       0.0,
       7.0,
       0.3562964200973511,
       1.01625657081604,
       0.0,
       0.5937553644180298,
       0.14297369122505188,
       225.0,
       5.876450061798096,
       2.6610124111175537,
       -48.0,
       0.2199999988079071,
       160.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       2.6610124111175537,
       1.0}},
     {55,
      {357378.9147338867, 112753.25337600708, 138.85936479317024, 0.0, 0.0, 0.0, 55.0},
      {2320.0,
       906.2056884765625,
       1196.0,
       2600.0,
       8.722057342529297,
       8.722057342529297,
       1.0,
       1.0,
       0.31220489740371704,
       1.2999999523162842,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       1.0},
      {6160.0,
       0.0,
       5204.0,
       1947.024658203125,
       4.06903076171875,
       4.06903076171875,
       1.0,
       2.0,
       0.7346076965332031,
       0.6499999761581421,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       1.0}}},
    {1984u,
     0.5,
     607307797u,
     120,
     205703.0,
     {62, 53, 5},
     {96,
      {651346.7512207031, 142997.92416203022, 686.7425301314797, 11940.45154593885, 6002.403133019805,
       7372.536202549934, 18.979171752929688},
      {4355.16015625,
       353.84002685546875,
       84.0,
       1838.6444091796875,
       0.16706186532974243,
       0.16706186532974243,
       2.0,
       2.0,
       0.3048999011516571,
       0.8775472044944763,
       -0.5,
       0.5337927341461182,
       0.10736444592475891,
       101.0,
       1.5096867084503174,
       2.061833143234253,
       -24.0,
       0.0,
       0.0,
       0.7172936797142029,
       0.3028102219104767,
       194.0,
       2.6429426670074463,
       1.4350614547729492,
       0.0,
       0.0,
       0.125},
      {3132.175537109375,
       0.5,
       6323.896484375,
       1002.5482788085938,
       1.444234013557434,
       39.18382263183594,
       0.0,
       7.0,
       0.3988910913467407,
       1.0507663488388062,
       0.0,
       0.6201812624931335,
       0.13836276531219482,
       118.0,
       5.431521415710449,
       0.30971473455429077,
       -56.0,
       0.2199999988079071,
       122.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.30971473455429077,
       1.0}},
     {53,
      {373274.68829345703, 110791.96743869781, 144.8479016814381, 0.0, 0.0, 0.0, 53.0},
      {5204.0,
       1025.4730224609375,
       84.0,
       2600.0,
       9.247846603393555,
       9.247846603393555,
       1.0,
       1.0,
       0.6511305570602417,
       1.2999999523162842,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       1.0},
      {84.0,
       0.0,
       3284.0,
       1924.4447021484375,
       5.376313209533691,
       5.376313209533691,
       1.0,
       2.0,
       0.7105469703674316,
       0.6499999761581421,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       1.0}}},
    {1984u,
     0.25,
     607307797u,
     90,
     152873.0,
     {41, 46, 3},
     {61,
      {444276.81022644043, 85661.27138492465, 441.4478114042431, 7842.5254271999, 4453.41768963635,
       4436.556181579828, -199.41329035162926},
      {4841.2216796875,
       294.85638427734375,
       2918.21826171875,
       1623.276123046875,
       0.23357342183589935,
       0.23357342183589935,
       2.0,
       5.0,
       0.3632858991622925,
       1.004820466041565,
       -0.5,
       0.5417240262031555,
       0.09443507343530655,
       67.0,
       0.3798435628414154,
       4.708941459655762,
       -51.0,
       0.0,
       0.0,
       0.7480498552322388,
       0.4463263154029846,
       199.0,
       0.19988542795181274,
       2.93794322013855,
       0.0,
       0.0,
       0.1666666716337204},
      {3132.175537109375,
       0.5,
       6323.896484375,
       1002.5482788085938,
       1.444234013557434,
       39.18382263183594,
       0.0,
       7.0,
       0.3988910913467407,
       1.0507663488388062,
       0.0,
       0.6201812624931335,
       0.13836276531219482,
       118.0,
       5.431521415710449,
       0.30971473455429077,
       -56.0,
       0.2199999988079071,
       122.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.30971473455429077,
       1.0}},
     {46,
      {317458.68829345703, 97300.10149383545, 131.1907003223896, 0.0, 0.0, 0.0, 46.0},
      {5204.0,
       1025.4730224609375,
       84.0,
       2600.0,
       9.247846603393555,
       9.247846603393555,
       1.0,
       1.0,
       0.6511305570602417,
       1.2999999523162842,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       1.0},
      {84.0,
       0.0,
       3284.0,
       1924.4447021484375,
       5.376313209533691,
       5.376313209533691,
       1.0,
       2.0,
       0.7105469703674316,
       0.6499999761581421,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       1.0}}},
    {1984u,
     0.02,
     607307797u,
     40,
     60226.0,
     {10, 28, 2},
     {23,
      {193188.79452514648, 34788.37779760361, 153.54821943864226, 2922.787534147501, 1094.440919354558,
       3478.905186653137, -153.57144816219807},
      {4841.2216796875,
       294.85638427734375,
       2918.21826171875,
       1623.276123046875,
       0.23357342183589935,
       0.23357342183589935,
       2.0,
       5.0,
       0.3632858991622925,
       1.004820466041565,
       -0.5,
       0.5417240262031555,
       0.09443507343530655,
       67.0,
       0.3798435628414154,
       4.708941459655762,
       -51.0,
       0.0,
       0.0,
       0.7480498552322388,
       0.4463263154029846,
       199.0,
       0.19988542795181274,
       2.93794322013855,
       0.0,
       0.0,
       0.1666666716337204},
      {4397.580078125,
       0.5,
       2493.930419921875,
       1410.9390869140625,
       1.5103161334991455,
       41.5847053527832,
       0.0,
       7.0,
       0.8698433041572571,
       1.057212471961975,
       0.0,
       0.6858454942703247,
       0.2973695695400238,
       192.0,
       5.789699077606201,
       1.7427778244018555,
       -43.0,
       0.2199999988079071,
       178.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       1.7427778244018555,
       1.0}},
     {28,
      {191358.68829345703, 62812.63578891754, 79.52632601186633, 0.0, 0.0, 0.0, 28.0},
      {5204.0,
       1025.4730224609375,
       84.0,
       2600.0,
       9.247846603393555,
       9.247846603393555,
       1.0,
       1.0,
       0.6511305570602417,
       1.2999999523162842,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       1.0},
      {84.0,
       0.0,
       3284.0,
       1924.4447021484375,
       5.376313209533691,
       5.376313209533691,
       1.0,
       2.0,
       0.7105469703674316,
       0.6499999761581421,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       0.0,
       1.0}}},
};

static const GoldenDir GOLDEN_DIR[] = {
    {1984u, 0, 0, 0.0, {-0.04984503376507113, 0.9757533126306317, 0.21312190290842353}},
    {1984u, 0, 0, 0.3, {0.1989928033109806, 0.9672034971448232, 0.1578583521428668}},
    {1984u, 0, 0, 0.77, {0.16259012158618397, 0.9463605692491759, -0.2792241489072276}},
    {1984u, 0, 46, 0.0, {0.05546603301764608, 0.8539449462643868, -0.5173988286909809}},
    {1984u, 0, 46, 0.3, {0.5587576489694077, 0.8063944714870773, -0.1936952401925703}},
    {1984u, 0, 46, 0.77, {0.4639531184245914, 0.8521032911100617, 0.24221371799198016}},
    {1984u, 0, 70, 0.0, {-0.3978134996266151, 0.8448926163942996, 0.3576323339369918}},
    {1984u, 0, 70, 0.3, {0.5891083554467275, 0.7784131301223631, -0.216850972780749}},
    {1984u, 0, 70, 0.77, {0.27598246987537206, 0.6568011357378136, -0.7017449283144178}},
    {1984u, 0, 139, 0.0, {0.45886169696865764, 0.8697601635407262, 0.18155770700428311}},
    {1984u, 0, 139, 0.3, {0.12432636303388148, 0.7262303667247273, 0.6761156778995999}},
    {1984u, 0, 139, 0.77, {0.41682909665184886, 0.8853281205387417, -0.2060282096406775}},
    {1984u, 1, 0, 0.0, {0.0, 1.0, 0.0}},
    {1984u, 1, 0, 0.3, {0.0, 1.0, 0.0}},
    {1984u, 1, 0, 0.77, {0.0, 1.0, 0.0}},
    {1984u, 1, 18, 0.0, {0.0, 1.0, 0.0}},
    {1984u, 1, 18, 0.3, {0.0, 1.0, 0.0}},
    {1984u, 1, 18, 0.77, {0.0, 1.0, 0.0}},
    {1984u, 1, 28, 0.0, {0.0, 1.0, 0.0}},
    {1984u, 1, 28, 0.3, {0.0, 1.0, 0.0}},
    {1984u, 1, 28, 0.77, {0.0, 1.0, 0.0}},
    {1984u, 1, 55, 0.0, {0.0, 1.0, 0.0}},
    {1984u, 1, 55, 0.3, {0.0, 1.0, 0.0}},
    {1984u, 1, 55, 0.77, {0.0, 1.0, 0.0}},
    {1u, 0, 0, 0.0, {0.5053123150481074, 0.5348498590000557, -0.6771964948139816}},
    {1u, 0, 0, 0.3, {0.2978484513089882, 0.8671535517241279, -0.39916289630308527}},
    {1u, 0, 0, 0.77, {0.7443072510926373, 0.5570611350265362, -0.36836070340614374}},
    {1u, 0, 52, 0.0, {0.451708414452442, 0.8785772744935797, -0.15511763618716584}},
    {1u, 0, 52, 0.3, {-0.4128433623175041, 0.8936018536213493, -0.1761706144476619}},
    {1u, 0, 52, 0.77, {-0.2063737548597394, 0.5815597766282343, 0.7868914153256537}},
    {1u, 0, 78, 0.0, {-0.5253799764415839, 0.6220173424550723, 0.5805775624663513}},
    {1u, 0, 78, 0.3, {-0.5237152592955707, 0.8309965215938, 0.1875289531778449}},
    {1u, 0, 78, 0.77, {-0.06392170920076927, 0.5269576226863573, 0.8474843237403245}},
    {1u, 0, 155, 0.0, {0.526399704727503, 0.734849891578856, 0.4276669121049011}},
    {1u, 0, 155, 0.3, {0.7365513084370593, 0.6453755998299102, -0.20244136233447843}},
    {1u, 0, 155, 0.77, {0.4096151459358501, 0.7986588822080584, -0.440862135015162}},
    {1u, 1, 0, 0.0, {0.0, 1.0, 0.0}},
    {1u, 1, 0, 0.3, {0.0, 1.0, 0.0}},
    {1u, 1, 0, 0.77, {0.0, 1.0, 0.0}},
    {1u, 1, 17, 0.0, {0.0, 1.0, 0.0}},
    {1u, 1, 17, 0.3, {0.0, 1.0, 0.0}},
    {1u, 1, 17, 0.77, {0.0, 1.0, 0.0}},
    {1u, 1, 25, 0.0, {0.0, 1.0, 0.0}},
    {1u, 1, 25, 0.3, {0.0, 1.0, 0.0}},
    {1u, 1, 25, 0.77, {0.0, 1.0, 0.0}},
    {1u, 1, 50, 0.0, {0.0, 1.0, 0.0}},
    {1u, 1, 50, 0.3, {0.0, 1.0, 0.0}},
    {1u, 1, 50, 0.77, {0.0, 1.0, 0.0}},
    {42u, 0, 0, 0.0, {-0.7428128280866942, 0.6524661271529985, 0.15005684038994882}},
    {42u, 0, 0, 0.3, {0.7505494718174917, 0.6524661271529985, -0.10470646241971758}},
    {42u, 0, 0, 0.77, {-0.5131490711866084, 0.8199736968507308, 0.2536161808979603}},
    {42u, 0, 40, 0.0, {-0.6828599533706767, 0.6451436491283008, 0.34277099654436494}},
    {42u, 0, 40, 0.3, {0.42266003872056807, 0.729627884430269, 0.5375887293559437}},
    {42u, 0, 40, 0.77, {0.2512740850847501, 0.9104223528069093, 0.328622083363783}},
    {42u, 0, 61, 0.0, {0.09026589909681672, 0.8269778552307132, 0.5549411630238406}},
    {42u, 0, 61, 0.3, {-0.6557503899539706, 0.5198668309635069, -0.547475939324438}},
    {42u, 0, 61, 0.77, {0.3725780597730112, 0.8356242371809716, 0.4036306772428204}},
    {42u, 0, 121, 0.0, {-0.47543539892564524, 0.8591484245902916, 0.18927537074969011}},
    {42u, 0, 121, 0.3, {0.5867009415077703, 0.7958915389305776, 0.14946124411569947}},
    {42u, 0, 121, 0.77, {-0.6150464287716861, 0.748456399411519, 0.2480542453479479}},
    {42u, 1, 0, 0.0, {0.0, 1.0, 0.0}},
    {42u, 1, 0, 0.3, {0.0, 1.0, 0.0}},
    {42u, 1, 0, 0.77, {0.0, 1.0, 0.0}},
    {42u, 1, 18, 0.0, {0.0, 1.0, 0.0}},
    {42u, 1, 18, 0.3, {0.0, 1.0, 0.0}},
    {42u, 1, 18, 0.77, {0.0, 1.0, 0.0}},
    {42u, 1, 27, 0.0, {0.0, 1.0, 0.0}},
    {42u, 1, 27, 0.3, {0.0, 1.0, 0.0}},
    {42u, 1, 27, 0.77, {0.0, 1.0, 0.0}},
    {42u, 1, 54, 0.0, {0.0, 1.0, 0.0}},
    {42u, 1, 54, 0.3, {0.0, 1.0, 0.0}},
    {42u, 1, 54, 0.77, {0.0, 1.0, 0.0}},
};

static const int WIDTH[7] = {3, 4, 4, 4, 4, 4, 4};

static const float *attr(const SwcBeamSet *s, int a) {
    const float *p[7] = {s->origin, s->shape, s->look, s->tilt, s->azim, s->fan, s->roll};
    return p[a];
}

static double sum_attr(const SwcBeamSet *s, int a) {
    double t = 0.0;
    for (int i = 0; i < s->count * WIDTH[a]; i++)
        t += (double)attr(s, a)[i];
    return t;
}

static void gather(const SwcBeamSet *s, int i, double out[FLOATS_PER_BEAM]) {
    int k = 0;
    for (int a = 0; a < 7; a++)
        for (int j = 0; j < WIDTH[a]; j++)
            out[k++] = (double)attr(s, a)[i * WIDTH[a] + j];
}

static double tol(double v) {
    return 1e-9 * (1.0 + fabs(v));
}

static void check_set(const SwcBeamSet *s, const GoldenSet *g) {
    CHECK_EQ_INT(s->count, g->count);
    for (int a = 0; a < 7; a++)
        CHECK_NEAR(sum_attr(s, a), g->sums[a], tol(g->sums[a]));
    if (s->count == 0)
        return;
    double v[FLOATS_PER_BEAM];
    gather(s, 0, v);
    for (int j = 0; j < FLOATS_PER_BEAM; j++)
        CHECK_NEAR(v[j], g->first[j], tol(g->first[j]));
    gather(s, s->count - 1, v);
    for (int j = 0; j < FLOATS_PER_BEAM; j++)
        CHECK_NEAR(v[j], g->last[j], tol(g->last[j]));
}

static void test_the_layer_seed_is_the_web_versions(void) {
    for (size_t g = 0; g < sizeof GOLDEN / sizeof GOLDEN[0]; g++)
        CHECK_EQ_INT((int)swc_beam_layer_seed(GOLDEN[g].seed), (int)GOLDEN[g].layer_seed);
}

static void test_matches_the_web_version(void) {
    for (size_t g = 0; g < sizeof GOLDEN / sizeof GOLDEN[0]; g++) {
        const Golden *e = &GOLDEN[g];
        swc_city_build(&city, e->seed);
        uint32_t ls = swc_beam_layer_seed(e->seed);
        swc_choose_emitters(&city, e->density, ls, lit);
        int n = 0;
        double s = 0.0;
        for (int i = 0; i < city.site_count; i++)
            if (lit[i]) {
                n++;
                s += (double)(i + 1);
            }
        CHECK_EQ_INT(n, e->lit_count);
        CHECK_NEAR(s, e->lit_sum, 0.0);
        swc_pack_beams(&city, e->density, ls, &beams);
        for (int k = 0; k < 3; k++)
            CHECK_EQ_INT(beams.sites_used[k], e->used[k]);
        check_set(&beams.sky, &e->sky);
        check_set(&beams.pillars, &e->pillars);
    }
}

static void test_the_direction_matches_the_web_version(void) {
    uint32_t seed = 0;
    for (size_t g = 0; g < sizeof GOLDEN_DIR / sizeof GOLDEN_DIR[0]; g++) {
        const GoldenDir *e = &GOLDEN_DIR[g];
        if (e->seed != seed) {
            seed = e->seed;
            swc_city_build(&city, seed);
            swc_pack_beams(&city, 1.0, swc_beam_layer_seed(seed), &beams);
        }
        double d[3];
        swc_beam_direction(e->set ? &beams.pillars : &beams.sky, e->index, e->loop, d);
        for (int j = 0; j < 3; j++)
            CHECK_NEAR(d[j], e->dir[j], 1e-9);
    }
}

/* An emitter as the packer sees it: a plaza ring of searchlights is one emitter at the centre of its cell. */
typedef struct Emitter {
    int cls; /* 0 searchlight (roof or ring), 1 plaza pillar, 2 laser */
    double x, z;
    int cell; /* ring cell, or -1 */
} Emitter;

static int collect_emitters(Emitter *out) {
    int n = 0;
    for (int i = 0; i < city.site_count; i++) {
        const SwcSite *s = &city.sites[i];
        if (!lit[i])
            continue;
        Emitter e;
        e.cell = -1;
        if (s->kind == SWC_SITE_SEARCHLIGHT && s->y >= 5.0) {
            e.cls = 0;
            e.x = s->x;
            e.z = s->z;
        } else if (s->kind == SWC_SITE_SEARCHLIGHT) {
            int cx = (int)floor(s->x / SWC_CELL), cz = (int)floor(s->z / SWC_CELL);
            e.cls = 0;
            e.cell = swc_cell_index(cx, cz);
            e.x = ((double)cx + 0.5) * SWC_CELL;
            e.z = ((double)cz + 0.5) * SWC_CELL;
            int seen = 0;
            for (int k = 0; k < n; k++)
                seen |= out[k].cell == e.cell;
            if (seen)
                continue;
        } else if (s->kind == SWC_SITE_PILLAR && s->y < 5.0) {
            e.cls = 1;
            e.x = s->x;
            e.z = s->z;
        } else if (s->kind == SWC_SITE_LASER) {
            e.cls = 2;
            e.x = s->x;
            e.z = s->z;
        } else {
            continue;
        }
        out[n++] = e;
    }
    return n;
}

/* Every lit emitter of a kind keeps its distance (wrap-aware), whatever the density. */
static void test_lit_emitters_keep_their_spacing(void) {
    static const double DENSITY[3] = {1.0, 0.5, 0.25};
    static const double SPACING[3] = {640.0, 240.0, 650.0};
    static Emitter em[SWC_MAX_SITES];
    swc_city_build(&city, 1984u);
    for (int d = 0; d < 3; d++) {
        swc_choose_emitters(&city, DENSITY[d], swc_beam_layer_seed(1984u), lit);
        int n = collect_emitters(em);
        int checked = 0;
        for (int i = 0; i < n; i++)
            for (int j = i + 1; j < n; j++) {
                if (em[i].cls != em[j].cls)
                    continue;
                double dist = hypot(swc_wrap_delta(em[i].x, em[j].x, SWC_TILE),
                                    swc_wrap_delta(em[i].z, em[j].z, SWC_TILE));
                CHECK(dist >= SPACING[em[i].cls] / sqrt(DENSITY[d]));
                checked++;
            }
        CHECK(checked > 100);
    }
}

static void test_megatower_pillars_are_always_lit(void) {
    swc_city_build(&city, 1984u);
    swc_choose_emitters(&city, 0.05, swc_beam_layer_seed(1984u), lit);
    int landmarks = 0;
    for (int i = 0; i < city.site_count; i++)
        if (city.sites[i].kind == SWC_SITE_PILLAR && city.sites[i].y > 5.0) {
            CHECK(lit[i]);
            landmarks++;
        }
    CHECK(landmarks > 0);
}

/* Every light of a plaza ring is lit or none is. */
static void test_a_plaza_ring_is_lit_whole(void) {
    swc_city_build(&city, 1984u);
    for (double density = 1.0; density > 0.2; density -= 0.25) {
        swc_choose_emitters(&city, density, swc_beam_layer_seed(1984u), lit);
        static int state[SWC_CELL_COUNT]; /* 0 unseen, 1 lit, 2 dark */
        memset(state, 0, sizeof state);
        int rings = 0;
        for (int i = 0; i < city.site_count; i++) {
            const SwcSite *s = &city.sites[i];
            if (s->kind != SWC_SITE_SEARCHLIGHT || s->y >= 5.0)
                continue;
            int cell = swc_cell_index((int)floor(s->x / SWC_CELL), (int)floor(s->z / SWC_CELL));
            int now = lit[i] ? 1 : 2;
            if (state[cell] == 0) {
                state[cell] = now;
                rings++;
            } else {
                CHECK_EQ_INT(state[cell], now);
            }
        }
        CHECK(rings > 3);
    }
}

static void test_a_lower_density_lights_fewer_emitters(void) {
    swc_city_build(&city, 1984u);
    int count[3];
    static const double DENSITY[3] = {1.0, 0.5, 0.25};
    for (int d = 0; d < 3; d++) {
        swc_choose_emitters(&city, DENSITY[d], swc_beam_layer_seed(1984u), lit);
        count[d] = 0;
        for (int i = 0; i < city.site_count; i++)
            count[d] += lit[i];
    }
    CHECK(count[0] > count[1] && count[1] > count[2]);
}

/* The same site gives the same beams whatever else is in the list: drop every site that is no emitter. */
static void test_a_sites_beams_do_not_depend_on_the_other_sites(void) {
    swc_city_build(&city, 42u);
    uint32_t ls = swc_beam_layer_seed(42u);
    swc_pack_beams(&city, 1.0, ls, &beams);
    memcpy(&trimmed, &city, sizeof city);
    int k = 0;
    for (int i = 0; i < city.site_count; i++) {
        uint8_t kind = city.sites[i].kind;
        if (kind == SWC_SITE_SEARCHLIGHT || kind == SWC_SITE_PILLAR || kind == SWC_SITE_LASER)
            trimmed.sites[k++] = city.sites[i];
    }
    trimmed.site_count = k;
    CHECK(k < city.site_count);
    swc_pack_beams(&trimmed, 1.0, ls, &beams2);
    CHECK_EQ_INT(beams.sky.count, beams2.sky.count);
    CHECK_EQ_INT(beams.pillars.count, beams2.pillars.count);
    CHECK(memcmp(beams.sky.origin, beams2.sky.origin, sizeof(float) * (size_t)beams.sky.count * 3) == 0);
    CHECK(memcmp(beams.sky.roll, beams2.sky.roll, sizeof(float) * (size_t)beams.sky.count * 4) == 0);
    CHECK(memcmp(beams.pillars.look, beams2.pillars.look, sizeof(float) * (size_t)beams.pillars.count * 4) ==
          0);
}

/* The packer's promises: searchlights lean 20 to 60 degrees at every moment, laser beams stay above the
 * horizon, pillars stand straight, and every direction is a unit vector. */
static void test_beams_stay_inside_their_limits(void) {
    swc_city_build(&city, 1984u);
    swc_pack_beams(&city, 1.0, swc_beam_layer_seed(1984u), &beams);
    int searchlights = 0, lasers = 0;
    for (int i = 0; i < beams.sky.count; i++) {
        int kind = (int)beams.sky.shape[i * 4 + 3];
        for (int t = 0; t < 40; t++) {
            double loop = (double)t / 40.0 + 0.0137, d[3];
            swc_beam_direction(&beams.sky, i, loop, d);
            CHECK_NEAR(d[0] * d[0] + d[1] * d[1] + d[2] * d[2], 1.0, 1e-9);
            if (kind == SWC_BEAM_SEARCHLIGHT)
                CHECK(d[1] >= cos(60.0 * PI / 180.0) - 1e-9 && d[1] <= cos(20.0 * PI / 180.0) + 1e-9);
            else
                CHECK(d[1] > cos(84.0 * PI / 180.0) - 1e-9);
        }
        searchlights += kind == SWC_BEAM_SEARCHLIGHT;
        lasers += kind == SWC_BEAM_LASER;
        CHECK(beams.sky.shape[i * 4] >= 600.0f && beams.sky.shape[i * 4] <= 2600.0f);
    }
    CHECK(searchlights > 20 && lasers > 5);
    for (int i = 0; i < beams.pillars.count; i++) {
        double d[3];
        swc_beam_direction(&beams.pillars, i, 0.37, d);
        CHECK_NEAR(d[1], 1.0, 1e-12);
        CHECK_EQ_INT((int)beams.pillars.shape[i * 4 + 3], SWC_BEAM_PILLAR);
    }
}

/* A plan never lights more beams than a set holds; the biggest of many seeds leaves room to spare. */
static void test_a_set_always_has_room(void) {
    int most_sky = 0, most_pillars = 0;
    for (uint32_t seed = 1; seed <= 24; seed++) {
        swc_city_build(&city, seed);
        swc_pack_beams(&city, 1.0, swc_beam_layer_seed(seed), &beams);
        if (beams.sky.count > most_sky)
            most_sky = beams.sky.count;
        if (beams.pillars.count > most_pillars)
            most_pillars = beams.pillars.count;
    }
    CHECK(most_sky > 0 && most_sky < SWC_BEAM_MAX / 2);
    CHECK(most_pillars > 0 && most_pillars < SWC_BEAM_MAX / 2);
}

static void test_packing_is_deterministic(void) {
    swc_city_build(&city, 7u);
    swc_pack_beams(&city, 1.0, swc_beam_layer_seed(7u), &beams);
    swc_pack_beams(&city, 1.0, swc_beam_layer_seed(7u), &beams2);
    CHECK_EQ_INT(beams.sky.count, beams2.sky.count);
    CHECK(memcmp(beams.sky.tilt, beams2.sky.tilt, sizeof(float) * (size_t)beams.sky.count * 4) == 0);
    CHECK(memcmp(beams.sky.azim, beams2.sky.azim, sizeof(float) * (size_t)beams.sky.count * 4) == 0);
    CHECK(memcmp(beams.sky.fan, beams2.sky.fan, sizeof(float) * (size_t)beams.sky.count * 4) == 0);
}

int main(void) {
    RUN(test_the_layer_seed_is_the_web_versions);
    RUN(test_matches_the_web_version);
    RUN(test_the_direction_matches_the_web_version);
    RUN(test_lit_emitters_keep_their_spacing);
    RUN(test_megatower_pillars_are_always_lit);
    RUN(test_a_plaza_ring_is_lit_whole);
    RUN(test_a_lower_density_lights_fewer_emitters);
    RUN(test_a_sites_beams_do_not_depend_on_the_other_sites);
    RUN(test_beams_stay_inside_their_limits);
    RUN(test_a_set_always_has_room);
    RUN(test_packing_is_deterministic);
    return TEST_SUMMARY();
}
