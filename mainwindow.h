#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCloseEvent>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_analyzeButton_clicked();
    void on_openButton_clicked();
    void on_saveButton_clicked();
    void on_exportButton_clicked();
    void on_sourceEdit_textChanged();

private:
    Ui::MainWindow *ui;
    bool maybeSave();
};

#endif // MAINWINDOW_H
