#pragma once
#include <QSplashScreen>

class QWidget;

// Simple startup splash: logo, name, author and GitHub link. Fades out once the main window is up.
class SplashScreen : public QSplashScreen
{
    Q_OBJECT
public:
    SplashScreen();
    void finishAfter(QWidget* mainWindow, int msecs);

protected:
    void drawContents(QPainter* painter) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void fadeOut();
    bool m_fading = false;
};
