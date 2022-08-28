#ifndef __LCDCTRL_H__
#define __LCDCTRL_H__

class LcdCtrl
{
public:
    void Setup(void);
    void ShowLoadingScreen(void);
    void ShowChStatusScreen(void);
    void UpdateChStatus(int ch_map);
};


#endif /* __LCDCTRL_H__ */
