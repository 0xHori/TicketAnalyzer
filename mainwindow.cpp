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
    ui->resultEdit->setReadOnly(true);

    connect(ui->sourceEdit, &QPlainTextEdit::textChanged,
            this, &MainWindow::on_sourceEdit_textChanged);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_analyzeButton_clicked()
{
    const QString query = ui->queryEdit->text().trimmed();
    if (query.size() > 100) {
        QMessageBox::warning(this, "Ошибка", "Длина запроса не должна превышать 100 единиц UTF-16");
        return;
    }

    const QString text = ui->sourceEdit->toPlainText();
    const QStringList lines = text.split('\n');

    qsizetype nonEmptyCount = 0;
    for (const QString &line : lines) {
        if (!line.trimmed().isEmpty()) {
            ++nonEmptyCount;
        }
    }

    if (nonEmptyCount > 10000) {
        QMessageBox::warning(this, "Ошибка", "Превышено ограничение: более 10 000 непустых строк");
        return;
    }

    qsizetype validCount = 0;
    qsizetype errorCount = 0;
    QStringList errors;
    QStringList matches;
    QStringList emails;

    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (lines[i].trimmed().isEmpty()) {
            continue;
        }

        QStringList fields;
        QString error;
        if (!parseTicket(lines[i], fields, error)) {
            ++errorCount;
            errors.append(QString("Строка %1: %2").arg(i + 1).arg(error));
            continue;
        }

        ++validCount;
        emails.append(fields[1]);

        if (!query.isEmpty() && fields[2].contains(query, Qt::CaseInsensitive)) {
            matches.append(fields.join(';'));
        }
    }

    QStringList report;
    report.append(QString("Непустых строк: %1").arg(nonEmptyCount));
    report.append(QString("Корректных записей: %1").arg(validCount));
    report.append(QString("Ошибочных записей: %1").arg(errorCount));
    report.append("");

    report.append("--- Найденные обращения ---");
    if (query.isEmpty()) {
        report.append("Поиск не задан (совпадений: 0)");
    } else if (matches.isEmpty()) {
        report.append("Нет");
    } else {
        report.append(matches.join('\n'));
    }
    report.append("");

    report.append("--- Список Email ---");
    if (emails.isEmpty()) {
        report.append("Нет");
    } else {
        report.append(emails.join('\n'));
    }
    report.append("");

    report.append("--- Список ошибок ---");
    if (errors.isEmpty()) {
        report.append("Нет");
    } else {
        report.append(errors.join('\n'));
    }

    ui->resultEdit->setPlainText(report.join('\n'));
}

void MainWindow::on_openButton_clicked()
{
    if (!maybeSave()) {
        return;
    }

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
    ui->resultEdit->clear();
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

void MainWindow::on_exportButton_clicked()
{
    const QString report = ui->resultEdit->toPlainText();

    if (report.trimmed().isEmpty()) {
        QMessageBox::warning(this, "Экспорт", "Отчёт пуст. Сначала выполните анализ.");
        return;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, "Экспорт отчёта", QString(), "Текст (*.txt)");
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!writeText(path, report, error)) {
        QMessageBox::warning(this, "Ошибка", error);
        return;
    }

    QMessageBox::information(this, "Успех", "Отчёт успешно сохранён.");
}

void MainWindow::on_sourceEdit_textChanged()
{
    ui->resultEdit->clear();
}

bool MainWindow::maybeSave()
{
    if (!ui->sourceEdit->document()->isModified()) {
        return true;
    }

    QMessageBox msgBox(this);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle("Сохранение");
    msgBox.setText("Исходный текст был изменён. Сохранить изменения?");
    QPushButton *saveBtn = msgBox.addButton("Сохранить", QMessageBox::AcceptRole);
    QPushButton *discardBtn = msgBox.addButton("Не сохранять", QMessageBox::DestructiveRole);
    msgBox.addButton("Отмена", QMessageBox::RejectRole);

    msgBox.exec();

    if (msgBox.clickedButton() == saveBtn) {
        on_saveButton_clicked();
        return !ui->sourceEdit->document()->isModified();
    } else if (msgBox.clickedButton() == discardBtn) {
        return true;
    }
    return false;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}