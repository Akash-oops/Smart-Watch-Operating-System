// Smartwatch OS — Phase 12C (Full)
// Features: Digital (Pikachu) + Analog (Spiderman) watch faces, Chat, Gallery, Steps, Heart, Bluetooth, Settings

#include <SFML/Graphics.hpp>
#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <ctime>
#include <vector>
#include <deque>
#include <mutex>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <algorithm>

using namespace std;
namespace fs = std::filesystem;

#define BT_PORT "\\\\.\\COM12"

struct Theme { sf::Color background; sf::Color text; };
enum class ScreenType { HOME, MENU, CHAT, GALLERY, STEPS, HEART, SETTINGS };
enum class WatchFace { DIGITAL, ANALOG };

/* -------------------- Base64 Decoder -------------------- */
static inline int b64val(char c){
    if(c>='A'&&c<='Z') return c-'A';
    if(c>='a'&&c<='z') return c-'a'+26;
    if(c>='0'&&c<='9') return c-'0'+52;
    if(c=='+') return 62;
    if(c=='/') return 63;
    if(c=='=') return -1;
    return -2;
}
static vector<uint8_t> base64_decode(const string& s){
    vector<uint8_t> out; out.reserve(s.size()*3/4);
    int val=0, valb=-8;
    for(unsigned char c : s){
        int d=b64val(c);
        if(d==-2) continue;
        if(d==-1) break;
        val=(val<<6)+d;
        valb+=6;
        if(valb>=0){
            out.push_back((uint8_t)((val>>valb)&0xFF));
            valb-=8;
        }
    }
    return out;
}

/* -------------------- Bluetooth Manager -------------------- */
class BluetoothManager {
public:
    HANDLE btHandle = INVALID_HANDLE_VALUE;
    atomic<bool> connected{false}, connecting{false};
    thread readThread, connectThread;

    deque<string> incoming; mutex inMutex;

    bool receivingFile = false;
    string incomingFilename;
    ofstream incomingFile;

    void attemptConnection(){
        connecting = true;
        btHandle = CreateFileA(BT_PORT, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if(btHandle != INVALID_HANDLE_VALUE){
            COMMTIMEOUTS t={0};
            t.ReadIntervalTimeout=50; t.ReadTotalTimeoutConstant=100;
            t.ReadTotalTimeoutMultiplier=10; t.WriteTotalTimeoutConstant=100;
            t.WriteTotalTimeoutMultiplier=10;
            SetCommTimeouts(btHandle,&t);

            DCB dcb={0}; dcb.DCBlength=sizeof(DCB);
            if(GetCommState(btHandle,&dcb)){
                dcb.BaudRate=CBR_9600; dcb.ByteSize=8; dcb.Parity=NOPARITY; dcb.StopBits=ONESTOPBIT;
                SetCommState(btHandle,&dcb);
            }
            connected = true;
            readThread = thread(&BluetoothManager::readLoop,this);
        }
        connecting = false;
    }

    void startConnectAsync(){
        if(connected||connecting) return;
        if(connectThread.joinable()) connectThread.join();
        connectThread = thread(&BluetoothManager::attemptConnection,this);
    }

    void sendMessage(const string& msg){
        if(!connected || btHandle==INVALID_HANDLE_VALUE) return;
        string m = msg + "\r\n";
        DWORD w; WriteFile(btHandle, m.c_str(), (DWORD)m.size(), &w, NULL);
    }

    void pushEvent(const string& s){
        lock_guard<mutex> lk(inMutex);
        incoming.push_back(s);
        if(incoming.size()>200) incoming.pop_front();
    }

    static string trim(string s){
        while(!s.empty() && (s.back()==' '||s.back()=='\t')) s.pop_back();
        size_t i=0; while(i<s.size() && (s[i]==' '||s[i]=='\t')) ++i;
        return s.substr(i);
    }

    void handleLine(const string& line){
        // File transfer protocol
        if(line.rfind("FILE_BEGIN:",0)==0){
            string fn = trim(line.substr(11));
            fs::create_directories("assets/gallery");
            string path = "assets/gallery/" + fn;
            incomingFile.open(path, ios::binary);
            incomingFilename = fn;
            receivingFile = true;
            return;
        }
        if(line.rfind("FILE_DATA:",0)==0){
            if(!receivingFile) return;
            string b64 = line.substr(10);
            auto bytes = base64_decode(b64);
            incomingFile.write((const char*)bytes.data(), (std::streamsize)bytes.size());
            return;
        }
        if(line=="FILE_END"){
            if(!receivingFile) return;
            incomingFile.close();
            receivingFile=false;
            pushEvent("GALLERY_SAVED: " + incomingFilename);
            return;
        }

        // Otherwise: chat text
        pushEvent("CHAT: " + line);
    }

    void readLoop(){
        char buffer[512]; DWORD br; string acc;
        while(connected){
            if(ReadFile(btHandle, buffer, sizeof(buffer)-1, &br, NULL)){
                if(br>0){
                    buffer[br]=0; acc += buffer;
                    size_t p;
                    while((p = acc.find('\n')) != string::npos){
                        string line = acc.substr(0, p);
                        acc.erase(0, p+1);
                        if(!line.empty() && line.back()=='\r') line.pop_back();
                        handleLine(line);
                    }
                }
            }
            this_thread::sleep_for(chrono::milliseconds(60));
        }
    }

    void disconnect(){
        if(connected){
            connected=false;
            if(readThread.joinable()) readThread.join();
            if(incomingFile.is_open()) incomingFile.close();
            CloseHandle(btHandle);
            btHandle = INVALID_HANDLE_VALUE;
        }
        if(connectThread.joinable()) connectThread.join();
    }
    ~BluetoothManager(){ disconnect(); }
};

/* -------------------- Text Wrap (for Chat) -------------------- */
vector<string> wrapText(const string&s, sf::Font& font, unsigned size, float maxW){
    vector<string> lines; sf::Text t("",font,size);
    size_t start=0;
    while(start<s.size()){
        size_t end=start; string best="";
        while(end<=s.size()){
            string sub=s.substr(start,end-start);
            t.setString(sub);
            if(t.getLocalBounds().width<=maxW){ best=sub; ++end; }
            else break;
        }
        if(best.empty()){ best = s.substr(start,1); end=start+1; }
        lines.push_back(best);
        start += best.size();
    }
    return lines;
}

/* -------------------- App -------------------- */
int main(){
    srand((unsigned)time(0));

    sf::RenderWindow win(sf::VideoMode(400,400), "Smartwatch OS");
    win.setFramerateLimit(60);

    Theme dark{sf::Color(10,10,10), sf::Color(255,255,255)};
    Theme light{sf::Color(240,240,240), sf::Color(30,30,30)};
    Theme theme = dark; bool isDark=true;

    sf::Font font;
    if(!font.loadFromFile("assets/fonts/Roboto-Regular.ttf")){
        cerr<<"Font missing at assets/fonts/Roboto-Regular.ttf\n";
        return 1;
    }

    BluetoothManager bt;
    ScreenType screen = ScreenType::HOME;
    WatchFace face = WatchFace::DIGITAL;

    // Watchface textures
    sf::Texture pikachuTex, spideyTex;
    pikachuTex.loadFromFile("assets/watchfaces/Pikachu.png");
    spideyTex.loadFromFile("assets/watchfaces/Spiderman.jpg");
    sf::Sprite faceSprite;

    // Home data (simulated sensors)
    int bpm = 75, steps = 0;
    sf::Clock bpmClock, stepClock, timeClock;

    // Home texts
    sf::Text txtBT("BT: OFF",font,16); txtBT.setPosition(10,10);
    sf::Text hintHome("Enter/M=Menu  B=Connect  T=Theme",font,14); hintHome.setPosition(40,370);

    sf::Text txtClock("",font,32); txtClock.setPosition(150,170);
    sf::Text txtDay("",font,22);   txtDay.setPosition(150,240);
    sf::Text txtDate("",font,20);  txtDate.setPosition(150,210);
    sf::Text txtBPM("",font,18);   txtBPM.setPosition(20,320);
    sf::Text txtSteps("",font,18); txtSteps.setPosition(40,290);

    auto updateTimeTexts = [&](){
        time_t now=time(nullptr); tm* l=localtime(&now);
        char tb[10], db[20], dy[20];
        strftime(tb,sizeof(tb),"%I:%M %p",l);
        strftime(dy,sizeof(dy),"%A",l);
        strftime(db,sizeof(db),"%d %b %Y",l);
        txtClock.setString(tb); txtDay.setString(dy); txtDate.setString(db);
    };
    updateTimeTexts(); timeClock.restart();

    // Analog hands
    sf::RectangleShape hourHand({4,60}), minHand({3,80}), secHand({2,90});
    hourHand.setOrigin(2,50); minHand.setOrigin(1.5,70); secHand.setOrigin(1,80);
    hourHand.setPosition(200,200); minHand.setPosition(200,200); secHand.setPosition(200,200);

    // Menu
    vector<string> items = {"Chat","Gallery","Steps Tracker","Heart Rate","Settings"};
    vector<sf::Text> menu; int sel=0;
    sf::Text head("=== MENU ===",font,26); head.setPosition(110,40);
    for(int i=0;i<(int)items.size();++i){ sf::Text t(items[i],font,22); t.setPosition(120,100+i*40); menu.push_back(t); }

    // Settings
    vector<string> settings = {"Toggle Theme", "Switch Watch Face"};
    int settingSel = 0;
    sf::Text settingTitle("=== SETTINGS ===",font,26); settingTitle.setPosition(90,40);

    // Chat
    sf::RectangleShape chatBox({360,230}); chatBox.setPosition(20,70);
    sf::RectangleShape inputBar({360,40}); inputBar.setPosition(20,310);
    sf::Text chatTitle("Chat",font,22); chatTitle.setPosition(170,35);
    string input; vector<string> history; bool cursor=true; sf::Clock cursorTimer;
    auto pushChat = [&](const string& who, const string& text){
        auto lines = wrapText(who + ": " + text, font, 18, 340);
        for(auto& l: lines){ history.push_back(l); if(history.size()>160) history.erase(history.begin()); }
    };

    // Gallery
    vector<string> imgs;
    if(fs::exists("assets/gallery")){
        for (const auto &entry : fs::directory_iterator("assets/gallery"))
            if(entry.is_regular_file()) imgs.push_back(entry.path().string());
    }
    size_t img=0; sf::Texture tex; sf::Sprite spr;
    bool loaded = (!imgs.empty() && tex.loadFromFile(imgs[0]));
    if(loaded) spr.setTexture(tex);
    sf::Clock galleryAutoRefresh; // auto refresh every 10s

    // Last message preview on home (optional)
    sf::Text lastMsg("",font,16); lastMsg.setPosition(20,330);

    // Main loop
    while(win.isOpen()){
        // Pull BT events
        {
            lock_guard<mutex> lk(bt.inMutex);
            while(!bt.incoming.empty()){
                string ev = bt.incoming.front(); bt.incoming.pop_front();
                if(ev.rfind("CHAT: ",0)==0){
                    string content = ev.substr(6);
                    pushChat("Phone", content);
                    lastMsg.setString(content);
                } else if(ev.rfind("GALLERY_SAVED:",0)==0){
                    string fn = BluetoothManager::trim(ev.substr(14));
                    string path = "assets/gallery/" + fn;
                    if(find(imgs.begin(), imgs.end(), path) == imgs.end()){
                        imgs.push_back(path);
                    }
                    if(screen == ScreenType::GALLERY){
                        img = imgs.size()-1;
                        loaded = tex.loadFromFile(imgs[img]);
                        if(loaded) spr.setTexture(tex);
                    }
                }
            }
        }

        // Events
        sf::Event e;
        while(win.pollEvent(e)){
            if(e.type==sf::Event::Closed) win.close();

            if(e.type==sf::Event::KeyPressed){
                // Backspace acts like Escape everywhere
                if(e.key.code==sf::Keyboard::BackSpace) e.key.code = sf::Keyboard::Escape;

                if(e.key.code==sf::Keyboard::T){ isDark=!isDark; theme=isDark?dark:light; }

                switch(screen){
                    case ScreenType::HOME:
                        if(e.key.code==sf::Keyboard::Enter || e.key.code==sf::Keyboard::M) screen=ScreenType::MENU;
                        if(e.key.code==sf::Keyboard::B) bt.startConnectAsync();
                        break;

                    case ScreenType::MENU:
                        if(e.key.code==sf::Keyboard::Up)   sel=(sel-1+(int)items.size())%(int)items.size();
                        if(e.key.code==sf::Keyboard::Down) sel=(sel+1)%(int)items.size();
                        if(e.key.code==sf::Keyboard::Enter){
                            if(items[sel]=="Chat") screen=ScreenType::CHAT;
                            else if(items[sel]=="Gallery") screen=ScreenType::GALLERY;
                            else if(items[sel]=="Steps Tracker") screen=ScreenType::STEPS;
                            else if(items[sel]=="Heart Rate") screen=ScreenType::HEART;
                            else if(items[sel]=="Settings") screen=ScreenType::SETTINGS;
                        }
                        if(e.key.code==sf::Keyboard::Escape) screen=ScreenType::HOME;
                        break;

                    case ScreenType::CHAT:
                        if(e.key.code==sf::Keyboard::Escape) screen=ScreenType::MENU;
                        if(e.key.code==sf::Keyboard::Enter && !input.empty()){
                            pushChat("You", input);
                            if(bt.connected) bt.sendMessage(input);
                            input.clear();
                        }
                        if(e.key.code==sf::Keyboard::BackSpace) { /* handled above as Esc */ }
                        if(e.key.code==sf::Keyboard::Delete) input.clear();
                        if(e.key.code==sf::Keyboard::BackSpace && !input.empty()) input.pop_back();
                        break;

                    case ScreenType::GALLERY:
                        if(e.key.code==sf::Keyboard::Escape) screen=ScreenType::MENU;
                        if(e.key.code==sf::Keyboard::R){
                            imgs.clear();
                            if(fs::exists("assets/gallery")){
                                for (const auto &entry : fs::directory_iterator("assets/gallery"))
                                    if(entry.is_regular_file()) imgs.push_back(entry.path().string());
                            }
                            if(!imgs.empty()){
                                img = imgs.size()-1;
                                loaded = tex.loadFromFile(imgs[img]);
                                if(loaded) spr.setTexture(tex);
                            }
                        }
                        if(!imgs.empty()){
                            if(e.key.code==sf::Keyboard::Left){
                                img = (img + imgs.size() - 1) % imgs.size();
                                loaded = tex.loadFromFile(imgs[img]); if(loaded) spr.setTexture(tex);
                            }
                            if(e.key.code==sf::Keyboard::Right){
                                img = (img + 1) % imgs.size();
                                loaded = tex.loadFromFile(imgs[img]); if(loaded) spr.setTexture(tex);
                            }
                        }
                        break;

                    case ScreenType::STEPS:
                        if(e.key.code==sf::Keyboard::Escape) screen=ScreenType::MENU;
                        if(e.key.code==sf::Keyboard::Space) steps += 15; // demo bump
                        break;

                    case ScreenType::HEART:
                        if(e.key.code==sf::Keyboard::Escape) screen=ScreenType::MENU;
                        break;

                    case ScreenType::SETTINGS:
                        if(e.key.code==sf::Keyboard::Up)   settingSel=(settingSel-1+(int)settings.size())%(int)settings.size();
                        if(e.key.code==sf::Keyboard::Down) settingSel=(settingSel+1)%(int)settings.size();
                        if(e.key.code==sf::Keyboard::Enter){
                            if(settings[settingSel]=="Toggle Theme") {
                                isDark=!isDark; theme=isDark?dark:light;
                            }
                            if(settings[settingSel]=="Switch Watch Face") {
                                face = (face==WatchFace::DIGITAL) ? WatchFace::ANALOG : WatchFace::DIGITAL;
                                // no extra action; draw branch will change immediately
                            }
                        }
                        if(e.key.code==sf::Keyboard::Escape) screen=ScreenType::MENU;
                        break;
                }
            }

            if(screen==ScreenType::CHAT && e.type==sf::Event::TextEntered){
                uint32_t u=e.text.unicode;
                if(u>=32 && u<127 && input.size()<160) input.push_back((char)u);
            }
        }

        // Simulated sensors
        if(bpmClock.getElapsedTime().asSeconds() > 2.f){
            bpm = std::clamp(bpm + (rand()%7 - 3), 60, 120);
            bpmClock.restart();
        }
        if(stepClock.getElapsedTime().asSeconds() > 1.2f){
            steps += rand()%5;
            stepClock.restart();
        }

        // Update time every second
        if(timeClock.getElapsedTime().asSeconds()>=1.f){
            updateTimeTexts();
            timeClock.restart();
        }

        // Auto-refresh gallery while in gallery
        if(screen==ScreenType::GALLERY && galleryAutoRefresh.getElapsedTime().asSeconds()>10.f){
            imgs.clear();
            if(fs::exists("assets/gallery")){
                for (const auto &entry : fs::directory_iterator("assets/gallery"))
                    if(entry.is_regular_file()) imgs.push_back(entry.path().string());
            }
            if(!imgs.empty()){
                img = imgs.size()-1;
                loaded = tex.loadFromFile(imgs[img]);
                if(loaded) spr.setTexture(tex);
            }
            galleryAutoRefresh.restart();
        }

        // Status
        txtBT.setString(bt.connected ? "BT: Connected" : (bt.connecting ? "BT: Connecting..." : "BT: OFF"));

        // Draw
        win.clear(theme.background);

        if(screen==ScreenType::HOME){
            if(face==WatchFace::DIGITAL){
                // Pikachu background scaled to window
                faceSprite.setTexture(pikachuTex);
                auto sz = pikachuTex.getSize();
                if(sz.x && sz.y){
                    float sc = min(400.f/sz.x, 400.f/sz.y);
                    faceSprite.setScale(sc, sc);
                    faceSprite.setPosition((400 - sz.x*sc)/2.f, (400 - sz.y*sc)/2.f);
                }
                win.draw(faceSprite);

                txtClock.setFillColor(theme.text);
                txtDay.setFillColor(theme.text);
                txtDate.setFillColor(theme.text);
                txtBPM.setFillColor(theme.text);
                txtSteps.setFillColor(theme.text);
                txtBT.setFillColor(theme.text);
                hintHome.setFillColor(theme.text);
                lastMsg.setFillColor(theme.text);

                txtBPM.setString("Heart: " + to_string(bpm) + " bpm");
                txtSteps.setString("Steps: " + to_string(steps));

                win.draw(txtClock); win.draw(txtDay); win.draw(txtDate);
                win.draw(txtBPM); win.draw(txtSteps);
                win.draw(txtBT); win.draw(hintHome);
                if(!lastMsg.getString().isEmpty()) win.draw(lastMsg);
            } else {
                // Spiderman background
                faceSprite.setTexture(spideyTex);
                auto sz = spideyTex.getSize();
                if(sz.x && sz.y){
                    float sc = min(400.f/sz.x, 400.f/sz.y);
                    faceSprite.setScale(sc, sc);
                    faceSprite.setPosition((400 - sz.x*sc)/2.f, (400 - sz.y*sc)/2.f);
                }
                win.draw(faceSprite);

                // Analog hands (no stats)
                time_t now=time(nullptr); tm* l=localtime(&now);
                float secAng = l->tm_sec * 6.f;
                float minAng = l->tm_min * 6.f + l->tm_sec * 0.1f;
                float hourAng = (l->tm_hour%12)*30.f + l->tm_min*0.5f;

                hourHand.setRotation(hourAng);
                minHand.setRotation(minAng);
                secHand.setRotation(secAng);

                hourHand.setFillColor(sf::Color::Black);
                minHand.setFillColor(sf::Color::Black);
                secHand.setFillColor(sf::Color(220,0,0));

                win.draw(hourHand); win.draw(minHand); win.draw(secHand);

                txtBT.setFillColor(theme.text);
                hintHome.setFillColor(theme.text);
                win.draw(txtBT); win.draw(hintHome);
            }
        }
        else if(screen==ScreenType::MENU){
            head.setFillColor(theme.text); win.draw(head);
            for(int i=0;i<(int)menu.size();++i){
                menu[i].setFillColor(i==sel?sf::Color(100,200,255):theme.text);
                win.draw(menu[i]);
            }
        }
        else if(screen==ScreenType::CHAT){
            chatBox.setFillColor(isDark?sf::Color(30,30,30,180):sf::Color(220,220,220,180));
            inputBar.setFillColor(isDark?sf::Color(15,15,15,220):sf::Color(210,210,210,220));
            chatTitle.setFillColor(theme.text);
            txtBT.setFillColor(theme.text);

            win.draw(chatBox); win.draw(inputBar); win.draw(chatTitle); win.draw(txtBT);

            float y=80;
            for(int i=max(0,(int)history.size()-11); i<(int)history.size(); ++i){
                sf::Text l(history[i],font,18);
                l.setFillColor(theme.text); l.setPosition(30,y);
                win.draw(l); y+=20;
            }
            string disp = input + (cursor ? "_" : " ");
            sf::Text it(disp,font,18); it.setFillColor(theme.text); it.setPosition(30,320); win.draw(it);

            if(cursorTimer.getElapsedTime().asMilliseconds()>500){ cursor=!cursor; cursorTimer.restart(); }
        }
        else if(screen==ScreenType::GALLERY){
            txtBT.setFillColor(theme.text); win.draw(txtBT);
            if(loaded){
                auto s=tex.getSize();
                if(s.x>0 && s.y>0){
                    float sc=min(400.f/s.x, 400.f/s.y);
                    spr.setScale(sc,sc);
                    spr.setPosition((400-s.x*sc)/2.f,(400-s.y*sc)/2.f);
                    win.draw(spr);
                }
            } else {
                sf::Text e("Image not found",font,18);
                e.setFillColor(theme.text); e.setPosition(110,180); win.draw(e);
            }
        }
        else if(screen==ScreenType::STEPS){
            txtBT.setFillColor(theme.text); win.draw(txtBT);
            sf::Text title("Steps Tracker", font, 22); title.setFillColor(theme.text); title.setPosition(120,40);
            sf::Text sTxt("Steps: " + to_string(steps), font, 28); sTxt.setFillColor(theme.text); sTxt.setPosition(120,150);

            int goal = 8000;
            float ratio = std::min(1.f, steps / float(goal));
            sf::RectangleShape bg({280.f, 16.f}); bg.setFillColor(isDark?sf::Color(40,40,40):sf::Color(210,210,210)); bg.setPosition(60,210);
            sf::RectangleShape fg({280.f*ratio, 16.f}); fg.setFillColor(sf::Color(100,200,255)); fg.setPosition(60,210);

            win.draw(title); win.draw(sTxt); win.draw(bg); win.draw(fg);

            sf::Text hint("Esc/Backspace=Back  Space=Add steps", font, 14); hint.setFillColor(theme.text); hint.setPosition(80,340);
            win.draw(hint);
        }
        else if(screen==ScreenType::HEART){
            txtBT.setFillColor(theme.text); win.draw(txtBT);
            sf::Text title("Heart Rate", font, 22); title.setFillColor(theme.text); title.setPosition(140,40);
            sf::Text bTxt("Current: " + to_string(bpm) + " bpm", font, 28); bTxt.setFillColor(theme.text); bTxt.setPosition(105,160);

            float r = 30.f + 6.f * std::sin( (float)(clock() % 10000) / 10000.f * 6.28318f * 2.f );
            sf::CircleShape pulse(r); pulse.setFillColor(sf::Color(100,200,255,180)); pulse.setOrigin(r,r); pulse.setPosition(200,110);

            win.draw(title); win.draw(pulse); win.draw(bTxt);

            sf::Text hint("Esc/Backspace=Back", font, 14); hint.setFillColor(theme.text); hint.setPosition(140,340);
            win.draw(hint);
        }
        else if(screen==ScreenType::SETTINGS){
            settingTitle.setFillColor(theme.text); win.draw(settingTitle);
            for(int i=0;i<(int)settings.size();++i){
                sf::Text t(settings[i],font,22);
                t.setFillColor(i==settingSel?sf::Color(100,200,255):theme.text);
                t.setPosition(100,120+i*40);
                win.draw(t);
            }
        }

        win.display();
    }

    bt.disconnect();
    return 0;
}
