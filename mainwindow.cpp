#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QRegularExpression>
#include <QStringList>

#include <QFile>
#include <QSaveFile>
#include <QTextStream>
#include <QStringConverter>

#include <QFileDialog>
#include <QMessageBox>

static bool parseTicket(const QString &line, QStringList &result, QString &error)
{
    error.clear();

    QStringList fields = line.split(';', Qt::KeepEmptyParts);
    if (fields.size() != 3) {
        error = "Ожидалось три поля";
        return false;
    }

    for (QString &field : fields) {
        field = field.trimmed();
    }

    const QRegularExpression numberRe(R"(\A[1-9][0-9]{2}\z)");
    const QRegularExpression emailRe(
        R"(\A[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\z)");

    if (!numberRe.match(fields[0]).hasMatch()) {
        error = "Номер должен быть от 100 до 999";
    } else if (fields[1].size() < 6 || fields[1].size() > 254
               || !emailRe.match(fields[1]).hasMatch()) {
        error = "Неверный email";
    } else if (fields[2].isEmpty() || fields[2].size() > 500) {
        error = "Описание должно содержать 1–500 единиц UTF-16";
    }

    if (!error.isEmpty()) {
        return false;
    }

    result = fields;
    return true;
}

static bool readText(const QString &path, QString &result, QString &error)
{
    error.clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        error = file.errorString();
        return false;
    }
    if (file.size() > 2097152) {
        error = "Файл превышает 2 МБ";
        return false;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    const QString temporary = stream.readAll();
    if (stream.status() != QTextStream::Ok) {
        error = "Ошибка чтения текста";
        return false;
    }
    result = temporary;
    return true;
}

static bool writeText(const QString &path, const QString &text, QString &error)
{
    error.clear();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        error = file.errorString();
        return false;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << text;
    stream.flush();
    if (stream.status() != QTextStream::Ok) {
        error = "Ошибка записи текста";
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        error = file.errorString();
        return false;
    }
    return true;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_analyzeButton_clicked()
{
    const QString text = ui->sourceEdit->toPlainText();
    const QStringList lines = text.split('\n', Qt::KeepEmptyParts);

    if (lines.isEmpty() || lines[0].trimmed().isEmpty()) {
        ui->resultEdit->setPlainText("Поле ввода пусто!");
        return;
    }

    QString testLine;
    for (const QString &l : lines) {
        if (!l.trimmed().isEmpty()) {
            testLine = l;
            break;
        }
    }

    QStringList fields;
    QString error;

    if (parseTicket(testLine, fields, error)) {
        ui->resultEdit->setPlainText(
            QString("Статус: УСПЕХ\nНомер: %1\nEmail: %2\nОписание: %3")
                .arg(fields[0], fields[1], fields[2]));
    } else {
        ui->resultEdit->setPlainText(
            QString("Статус: ОШИБКА\nПричина: %1").arg(error));
    }
}

void MainWindow::on_openButton_clicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Открыть текст", QString(), "Текст (*.txt)");
    if (path.isEmpty()) {
        return;
    }

    QString text, error;
    if (!readText(path, text, error)) {
        QMessageBox::warning(this, "Ошибка", error);
        return;
    }

    ui->sourceEdit->setPlainText(text);
    ui->sourceEdit->document()->setModified(false);
}

void MainWindow::on_saveButton_clicked()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "Сохранить текст", QString(), "Текст (*.txt)");
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!writeText(path, ui->sourceEdit->toPlainText(), error)) {
        QMessageBox::warning(this, "Ошибка", error);
        return;
    }

    ui->sourceEdit->document()->setModified(false);
    QMessageBox::information(this, "Успех", "Файл успешно сохранён!");
}
