#include <M5StickCPlus2.h>

const int LCD_WIDTH = 240;
const int LCD_HEIGHT = 135;
const int SAMPLES = 240;
const int DOTS_DIV = 10;

const int ad_ch0 = 26; // Analog 26 pin for channel 0
const int ad_ch1 = 36; // Analog 36 pin for channel 1
const long VREF[] = { 250, 500, 1250, 2500, 5000 };
const int MILLIVOL_per_dot[] = { 33, 17, 6, 3, 2 };
const int MODE_ON = 0;
const int MODE_INV = 1;
const int MODE_OFF = 2;
const char *Modes[] = { "NORM", "INV", "OFF" };
const int TRIG_AUTO = 0;
const int TRIG_NORM = 1;
const int TRIG_SCAN = 2;
const char *TRIG_Modes[] = { "Auto", "Norm", "Scan" };
const int TRIG_E_UP = 0;
const int TRIG_E_DN = 1;
#define RATE_MIN 0
#define RATE_MAX 13
const char *Rates[] = { "F1-1", "F1-2", "  F2", " 5ms", "10ms", "20ms", "50ms", "0.1s", "0.2s", "0.5s", "1s", "2s", "5s", "10s" };
short rate = 3;
#define RANGE_MIN 0
#define RANGE_MAX 4
const char *Ranges[] = { " 1V", "0.5V", "0.2V", "0.1V", "50mV" };
int range0 = RANGE_MIN;
short range1 = RANGE_MIN;
short ch0_mode = MODE_ON;
short ch1_mode = MODE_ON;
int ch0_off = 0;
int ch1_off = 0;
short trig_mode = TRIG_AUTO;
int trig_lv = 40;
short trig_edge = TRIG_E_UP;
short trig_ch = 0;
short Start = 1;
short menu = 19;
short data[4][SAMPLES]; // keep twice of the number of channels to make it a double buffer
short sample = 0;       // index for double buffer
int amplitude = 0;
int amplitudeStep = 5;

TaskHandle_t LedC_Gen;
TaskHandle_t SigmaDeltaGen;

///////////////////////////////////////////////////////////////////////////////////////////////
#define CH1COLOR YELLOW
#define CH2COLOR CYAN
#define GREY 0x7BEF

int lastmenu = 0;
void DrawText()
{
    M5.Lcd.setRotation(0);
    if (menu > 19)
    {
        M5.Lcd.fillRect(5, menu - 10, 40, 10, BLACK);
    }
    else if (menu == 19)
    {
        M5.Lcd.fillRect(5, 129, 40, 10, BLACK);
    }
    
    M5.Lcd.fillRect(5, menu, 40, 10, BLUE);
    
    M5.Lcd.drawString((Start == 1 ? "Stop" : "Run"), 5, 20);
    M5.Lcd.drawString(String(String(Ranges[range0]) + "/DIV"), 5, 30);
    M5.Lcd.drawString(String(String(Ranges[range1]) + "/DIV"), 5, 40);
    M5.Lcd.drawString(String(String(Rates[rate]) + "/DIV"), 5, 50);
    M5.Lcd.drawString(String(Modes[ch0_mode]), 5, 60);
    M5.Lcd.drawString(String(Modes[ch1_mode]), 5, 70);
    
    M5.Lcd.drawString(String("OFS1:" + String(ch0_off)), 5, 80);
    M5.Lcd.drawString(String("OFS2:" + String(ch1_off)), 5, 90);
    M5.Lcd.drawString(String(trig_ch == 0 ? "T:1" : "T:2"), 5, 100);
    M5.Lcd.drawString(String(TRIG_Modes[trig_mode]), 5, 110);
    M5.Lcd.drawString(String("Tlv:" + String(trig_lv)), 5, 120);
    M5.Lcd.drawString(String((trig_edge == TRIG_E_UP) ? "T:UP" : "T:DN"), 5, 130);
}

void CheckSW()
{
    M5.update();
    if (M5.BtnA.wasPressed())
    {
        (menu < 129) ? (menu += 10) : (menu = 19);
        DrawText();
        return;
    }

    if (M5.BtnB.wasPressed())
    {
        switch (menu)
        {
        case 19:
            Start = !Start;
            break;
        case 29:
            if (++range0 > RANGE_MAX)
                range0 = RANGE_MIN;
            break;
        case 39:
            if (++range1 > RANGE_MAX)
                range1 = RANGE_MIN;
            break;
        case 49:
            if (++rate > RATE_MAX)
                rate = RATE_MIN;
            break;
        case 59:
            if (++ch0_mode > MODE_OFF)
                ch0_mode = MODE_ON;
            break;
        case 69:
            if (++ch1_mode > MODE_OFF)
                ch1_mode = MODE_ON;
            break;
        case 79:
            if (ch0_off < 4095)
                ch0_off += 4096 / VREF[range0];
            else
                ch0_off = 0;
            break;
        case 89:
            if (ch1_off < 4095)
                ch1_off += 4096 / VREF[range1];
            else
                ch1_off = 0;
            break;
        case 99:
            trig_ch = !trig_ch;
            break;
        case 109:
            if (++trig_mode > TRIG_SCAN)
                trig_mode = TRIG_AUTO;
            break;
        case 119:
            if (++trig_lv > 60)
                trig_lv = 0;
            break;
        case 129:
            trig_edge = !trig_edge;
            break;
        }
        DrawText();
        return;
    }
}

void DrawGrid()
{
    for (int x = 0; x <= SAMPLES; x += 2) // Horizontal Line
    {
        for (int y = 0; y <= LCD_HEIGHT; y += DOTS_DIV)
        {
            M5.Lcd.drawPixel(x, y, GREY);
            CheckSW();
        }
        if (LCD_HEIGHT == 80)
        {
            M5.Lcd.drawPixel(x, LCD_HEIGHT - 1, GREY);
        }
    }
    for (int x = 0; x <= SAMPLES; x += DOTS_DIV) // Vertical Line
    {
        for (int y = 0; y <= LCD_HEIGHT; y += 2)
        {
            M5.Lcd.drawPixel(x, y, GREY);
            CheckSW();
        }
    }
}

void DrawGrid(int x)
{
    if ((x % 2) == 0)
    {
        for (int y = 0; y <= LCD_HEIGHT; y += DOTS_DIV)
        {
            M5.Lcd.drawPixel(x, y, GREY);
        }
    }
    if ((x % DOTS_DIV) == 0)
    {
        for (int y = 0; y <= LCD_HEIGHT; y += 2)
        {
            M5.Lcd.drawPixel(x, y, GREY);
        }
    }
}

void ClearAndDrawGraph()
{
    int clear = 0;

    if (sample == 0)
    {
        clear = 2;
    }
    for (int x = 0; x < (SAMPLES - 1); x++)
    {
        M5.Lcd.drawLine(x, LCD_HEIGHT - data[clear + 0][x], x + 1, LCD_HEIGHT - data[clear + 0][x + 1], BLACK);
        M5.Lcd.drawLine(x, LCD_HEIGHT - data[clear + 1][x], x + 1, LCD_HEIGHT - data[clear + 1][x + 1], BLACK);
        if (ch0_mode != MODE_OFF)
        {
            M5.Lcd.drawLine(x, LCD_HEIGHT - data[sample + 0][x], x + 1, LCD_HEIGHT - data[sample + 0][x + 1], CH1COLOR);
        }
        if (ch1_mode != MODE_OFF)
        {
            M5.Lcd.drawLine(x, LCD_HEIGHT - data[sample + 1][x], x + 1, LCD_HEIGHT - data[sample + 1][x + 1], CH2COLOR);
        }
        CheckSW();
    }
}

void ClearAndDrawDot(int i)
{
    int clear = 0;

    if (i <= 1)
    {
        return;
    }
    if (sample == 0)
    {
        clear = 2;
    }

    M5.Lcd.drawLine(i - 1, LCD_HEIGHT - data[clear + 0][i - 1], i, LCD_HEIGHT - data[clear + 0][i], BLACK);
    M5.Lcd.drawLine(i - 1, LCD_HEIGHT - data[clear + 1][i - 1], i, LCD_HEIGHT - data[clear + 1][i], BLACK);
    if (ch0_mode != MODE_OFF)
    {
        M5.Lcd.drawLine(i - 1, LCD_HEIGHT - data[sample + 0][i - 1], i, LCD_HEIGHT - data[sample + 0][i], CH1COLOR);
    }
    if (ch1_mode != MODE_OFF)
    {
        M5.Lcd.drawLine(i - 1, LCD_HEIGHT - data[sample + 1][i - 1], i, LCD_HEIGHT - data[sample + 1][i], CH2COLOR);
    }

    if ((i % DOTS_DIV) == 0)
    {
        M5.Lcd.drawPixel(i, LCD_HEIGHT - 1, GREY);
    }
}

void setup()
{
    M5.begin();
    M5.Lcd.setRotation(1);
    M5.Lcd.fillScreen(BLACK);
    DrawGrid();
    DrawText();

    pinMode(ad_ch0, INPUT);
    pinMode(ad_ch1, INPUT);
}

void loop()
{
    CheckSW();

    if (Start == 1)
    {
        for (int i = 0; i < SAMPLES; i++)
        {
            CheckSW();
            data[sample + 0][i] = (ch0_mode == MODE_ON ? analogRead(ad_ch0) : 2048) / VREF[range0];
            data[sample + 1][i] = (ch1_mode == MODE_ON ? analogRead(ad_ch1) : 2048) / VREF[range1];

            if ((i % 2) == 0)
            {
                DrawGrid(i);
            }
            if (i > 0)
            {
                ClearAndDrawDot(i);
            }
        }
        sample ^= 2;
    }
    else
    {
        delay(100);
    }
}
