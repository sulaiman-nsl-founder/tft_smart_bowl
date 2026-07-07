#ifndef APP_APPLICATION_H
#define APP_APPLICATION_H

class Application {
public:
    static Application& getInstance();

    void setup();
    void loop();

private:
    Application() = default;
    ~Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
};

#endif // APP_APPLICATION_H
