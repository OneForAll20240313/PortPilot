#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("PortPilot"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));

    // TODO: 挂载 UI 层主窗口（串口调试工作台）
    QMessageBox::information(nullptr,
        QStringLiteral("PortPilot"),
        QStringLiteral("工程骨架已就绪，UI 层待实现。"));

    return 0;
}