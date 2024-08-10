#ifndef INC_CHM_SHE_CALCULATE_H_
#define INC_CHM_SHE_CALCULATE_H_

extern const float _7Alpha[1274][7];
extern const float _7WideAlpha[275][7];
extern const float _7Alpha_Old[1274][7];
extern const float _6Alpha[1274][6];
extern const float _6Alpha_Old[1274][6];
extern const float _6WideAlpha[274][6];
extern const float _5Alpha[1274][5];
extern const float _5Alpha_Old[1274][5];
extern const float _5WideAlpha[274][5];
extern const float _4Alpha[1274][4];
extern const float _4WideAlpha[475][4];
extern const float _3Alpha[1274][3];
extern const float _3WideAlpha[471][3];
extern const float _2Alpha[1274][2];
extern const float _2WideAlpha[471][2];
extern const float _WideAlpha[636];
extern const char _7Alpha_Polary[];
extern const char _7OldAlpha_Polary[];
extern const char _6Alpha_Polary[];
extern const char _6OldAlpha_Polary[];
extern const char _5Alpha_Polary[];
extern const char _5OldAlpha_Polary[];
extern const char _4Alpha_Polary[];
extern const char _3Alpha_Polary[];
extern const char _2Alpha_Polary[];
extern const float _5Alpha_SHE[1271][5];
extern const float _3Alpha_SHE[1274][3];
extern const float _2Alpha_SHE[1274][2];
extern const float _1Alpha_SHE[1274];


int get_P_with_switchingangle(
	float alpha1,
	float alpha2,
	float alpha3,
	float alpha4,
	float alpha5,
	float alpha6,
	float alpha7,
	int flag,
	float x);

#endif /* INC_CHM_SHE_CALCULATE_H_ */
