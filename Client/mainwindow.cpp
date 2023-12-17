#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    socket = new QTcpSocket(this);

    connect(this, &MainWindow::newMessage, this, &MainWindow::displayMessage);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::readSocket);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::discardSocket);
    connect(socket, &QAbstractSocket::errorOccurred, this, &MainWindow::displayError);

    socket->connectToHost(QHostAddress::LocalHost, 8080);

    if (socket->waitForConnected())
        ui->statusBar->showMessage("서버에 연결되었습니다.");
    else {
        QMessageBox::critical(this, "QTCPClient", QString("오류가 발생했습니다: %1.").arg(socket->errorString()));
        exit(EXIT_FAILURE);
    }
}

MainWindow::~MainWindow()
{
    if (socket->isOpen())
        socket->close();
    delete ui;
}

void MainWindow::readSocket()
{
    QByteArray buffer;

    QDataStream socketStream(socket);
    socketStream.setVersion(QDataStream::Qt_5_15);

    socketStream.startTransaction();
    socketStream >> buffer;

    if (!socketStream.commitTransaction())
    {
        QString message = QString("%1 :: 데이터를 기다리는 중..").arg(socket->socketDescriptor());
        emit newMessage(message);
        return;
    }

    QString header = buffer.mid(0, 128);
    QString fileType = header.split(",")[0].split(":")[1];

    buffer = buffer.mid(128);

    if (fileType == "attachment") { // 메시지 유형이 "첨부파일"인 경우
        QString fileName = header.split(",")[1].split(":")[1];
        QString ext = fileName.split(".")[1];
        QString size = header.split(",")[2].split(":")[1].split(";")[0];

        if (QMessageBox::Yes == QMessageBox::question(this, "QTCPServer", QString("사용자가 보낸 파일을 받으시겠습니까?").arg(socket->socketDescriptor()).arg(size).arg(fileName)))
        {
            QString filePath = QFileDialog::getSaveFileName(this, tr("Save File"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/" + fileName, QString("File (*.%1)").arg(ext));

            QFile file(filePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(buffer);
                QString message = QString("%2 사용자가 보낸 파일이 해당 경로에 저장되었습니다.").arg(socket->socketDescriptor()).arg(QString(filePath));
                emit newMessage(message);
            }
            else // 오류 발생
                QMessageBox::critical(this, "QTCPServer", "첨부 파일 저장 과정에서 오류가 발생했습니다.");
        }
        else {// 파일을 받지 않는 경우
            QString message = QString("첨부 파일이 삭제되었습니다.").arg(socket->socketDescriptor());
            emit newMessage(message);
        }
    }
    else if (fileType == "message") {// 메시지 유형이 "메시지"인 경우
        QString message = QString("%1 :: %2").arg(socket->socketDescriptor()).arg(QString::fromStdString(buffer.toStdString()));
        emit newMessage(message);
    }
}

void MainWindow::discardSocket()// 소켓 삭제
{
    socket->deleteLater();
    socket = nullptr;

    ui->statusBar->showMessage("사용자가 나갔습니다.");
}

void MainWindow::displayError(QAbstractSocket::SocketError socketError)
{
    switch (socketError) {
    case QAbstractSocket::RemoteHostClosedError:
        break;
    case QAbstractSocket::HostNotFoundError:
        QMessageBox::information(this, "QTCPServer", "호스트를 찾을 수 없습니다.");
        break;
    case QAbstractSocket::ConnectionRefusedError:
        QMessageBox::information(this, "QTCPServer", "호스트 이름 및 포트 설정이 올바른지 확인");
        break;
    default:
        QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
        QMessageBox::information(this, "QTCPServer", QString("오류가 발생했습니다.: %1.").arg(socket->errorString()));
        break;
    }
}
void MainWindow::on_pushButton_sendMessage_clicked()
{
    if (socket)
    {
        if (socket->isOpen())
        {
            QString str = ui->lineEdit_message->text();
            ui->textBrowser_receivedMessages->append(str);
            QDataStream socketStream(socket);
            socketStream.setVersion(QDataStream::Qt_5_15);

            QByteArray header;
            header.prepend(QString("fileType:message,fileName:null,fileSize:%1;").arg(str.size()).toUtf8());
            header.resize(128);

            QByteArray byteArray = str.toUtf8();
            byteArray.prepend(header);

            socketStream << byteArray;

            ui->lineEdit_message->clear();
        }
        else
            QMessageBox::critical(this, "QTCPClient", "소켓이 열려있지 않습니다.");
    }
    else
        QMessageBox::critical(this, "QTCPClient", "연결되지 않았습니다.");
}

void MainWindow::on_pushButton_sendAttachment_clicked()
{
    if (socket)
    {
        if (socket->isOpen())
        {
            QString filePath = QFileDialog::getOpenFileName(this, ("파일을 선택하세요"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation), ("File (*.json *.txt *.png *.jpg *.jpeg)"));

            if (filePath.isEmpty()) {
                QMessageBox::critical(this, "QTCPClient", "첨부파일이 선택되지 않았습니다.");
                return;
            }

            QFile m_file(filePath);
            if (m_file.open(QIODevice::ReadOnly)) {

                QFileInfo fileInfo(m_file.fileName());
                QString fileName(fileInfo.fileName());

                QDataStream socketStream(socket);
                socketStream.setVersion(QDataStream::Qt_5_15);

                QByteArray header;
                header.prepend(QString("fileType:attachment,fileName:%1,fileSize:%2;").arg(fileName).arg(m_file.size()).toUtf8());
                header.resize(128);

                QByteArray byteArray = m_file.readAll();
                byteArray.prepend(header);

                socketStream.setVersion(QDataStream::Qt_5_15);
                socketStream << byteArray;
            }
            else
                QMessageBox::critical(this, "QTCPClient", "첨부파일을 열 수 없습니다.");
        }
        else
            QMessageBox::critical(this, "QTCPClient", "소켓이 열려있지 않습니다.");
    }
    else
        QMessageBox::critical(this, "QTCPClient", "연결되지 않았습니다.");
}

void MainWindow::displayMessage(const QString& str)
{
    ui->textBrowser_receivedMessages->append(str);
}