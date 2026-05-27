#include "mainwindow.h"
#include "gifprocessor.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QImageReader>
#include <QMessageBox>
#include <QPainter>
#include <QResizeEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_movie(nullptr),
      m_isPreviewLoaded(false) {
  ui->setupUi(this);
  setupUI();

  // 设置窗口图标
  setWindowIcon(QIcon(":/app.ico")); // 如果使用资源文件

  m_processor = new GifProcessor();
  m_processor->moveToThread(&m_workerThread);

  connect(&m_workerThread, &QThread::finished, m_processor,
          &QObject::deleteLater);
  connect(m_processor, &GifProcessor::progressUpdated, this,
          &MainWindow::onProgressUpdated);
  connect(m_processor, &GifProcessor::statusMessage, this,
          &MainWindow::onStatusMessage);
  connect(m_processor, &GifProcessor::finished, this, &MainWindow::onFinished);
  connect(m_processor, &GifProcessor::error, this, &MainWindow::onError);
  connect(m_processor, &GifProcessor::sizeWarning, this,
          &MainWindow::onSizeWarning); // 新增

  m_workerThread.start();
}

MainWindow::~MainWindow() {
  if (m_movie) {
    m_movie->stop();
    delete m_movie;
  }
  m_workerThread.quit();
  m_workerThread.wait();
  delete ui;
}

void MainWindow::setupUI() {
  setWindowTitle("Steam GIF Cutter - 创意工坊切割工具");
  setMinimumSize(1000, 800);
  resize(1100, 900);

  m_outputDir = QDir::current().absolutePath() + "/steam_output";
  ui->outputEdit->setText(m_outputDir);

  // 设置预览标签
  ui->previewLabel->setAlignment(Qt::AlignCenter);
  ui->previewLabel->setMinimumSize(600, 400);

  connect(ui->fileButton, &QPushButton::clicked, this,
          &MainWindow::selectInputFile);
  connect(ui->outputButton, &QPushButton::clicked, this,
          &MainWindow::selectOutputDir);
  connect(ui->startButton, &QPushButton::clicked, this,
          &MainWindow::startProcess);
}

void MainWindow::selectInputFile() {
  QString filePath =
      QFileDialog::getOpenFileName(this, "选择GIF文件", "", "GIF文件 (*.gif)");

  if (!filePath.isEmpty()) {
    m_inputPath = filePath;
    ui->fileEdit->setText(filePath);
    ui->startButton->setEnabled(true);
    loadPreview();
  }
}

void MainWindow::selectOutputDir() {
  QString dirPath =
      QFileDialog::getExistingDirectory(this, "选择输出目录", m_outputDir);

  if (!dirPath.isEmpty()) {
    m_outputDir = dirPath;
    ui->outputEdit->setText(dirPath);
  }
}

void MainWindow::loadPreview() {
  if (m_movie) {
    disconnect(m_movie, nullptr, this, nullptr);
    m_movie->stop();
    delete m_movie;
    m_movie = nullptr;
  }

  // 使用 QImageReader 获取正确的 GIF 尺寸
  QImageReader reader(m_inputPath);
  m_originalSize = reader.size();

  if (m_originalSize.isEmpty() || m_originalSize.width() <= 0) {
    // 如果还是获取不到，尝试用 QMovie
    QMovie tempMovie(m_inputPath);
    tempMovie.jumpToFrame(0);
    m_originalSize = tempMovie.frameRect().size();
  }

  // 如果还是无效，使用默认值
  if (m_originalSize.isEmpty() || m_originalSize.width() <= 0) {
    m_originalSize = QSize(1000, 651);
  }

  qDebug() << "GIF 原始尺寸:" << m_originalSize.width() << "x"
           << m_originalSize.height();

  // 创建新的 Movie
  m_movie = new QMovie(m_inputPath);

  if (!m_movie->isValid()) {
    ui->previewLabel->setText("无法加载GIF文件");
    return;
  }

  // 连接帧变化信号
  connect(m_movie, &QMovie::frameChanged, this, &MainWindow::updatePreview);

  // 启动循环播放
  m_movie->start();
  m_isPreviewLoaded = true;

  // 调整预览区域大小
  resizePreview();

  qDebug() << "预览加载完成，总帧数:" << m_movie->frameCount();
}

void MainWindow::resizePreview() {
  if (!m_isPreviewLoaded)
    return;

  // 获取预览区域可用大小
  int availableWidth = ui->previewGroupBox->width() - 20;
  // int availableHeight = 500;
  int availableHeight = ui->previewGroupBox->height() - 20;

  // 计算缩放后的尺寸（保持比例）
  int displayWidth = m_originalSize.width();
  int displayHeight = m_originalSize.height();

  if (displayWidth > availableWidth) {
    double ratio = (double)availableWidth / displayWidth;
    displayWidth = availableWidth;
    displayHeight = (int)(m_originalSize.height() * ratio);
  }

  if (displayHeight > availableHeight) {
    double ratio = (double)availableHeight / displayHeight;
    displayHeight = availableHeight;
    displayWidth = (int)(displayWidth * ratio);
  }

  ui->previewLabel->setFixedSize(displayWidth, displayHeight);
  qDebug() << "预览标签大小:" << displayWidth << "x" << displayHeight;
}

void MainWindow::updatePreview() {
  if (!m_movie || !m_movie->isValid())
    return;

  QPixmap originalFrame = m_movie->currentPixmap();
  if (originalFrame.isNull())
    return;

  // 如果原始帧尺寸和记录的不一致，更新记录
  if (originalFrame.size() != m_originalSize && originalFrame.width() > 0) {
    m_originalSize = originalFrame.size();
    qDebug() << "更新原始尺寸:" << m_originalSize.width() << "x"
             << m_originalSize.height();
    resizePreview();
  }

  // 复制并绘制切割线
  QPixmap frameWithLines = originalFrame;
  drawCutLines(frameWithLines);

  // 缩放到标签大小
  QSize targetSize = ui->previewLabel->size();
  if (targetSize.width() > 0 && targetSize.height() > 0) {
    QPixmap scaled = frameWithLines.scaled(targetSize, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation);
    ui->previewLabel->setPixmap(scaled);
  } else {
    ui->previewLabel->setPixmap(frameWithLines);
  }
}

void MainWindow::drawCutLines(QPixmap &pixmap) {
  if (pixmap.isNull())
    return;

  QPainter painter(&pixmap);
  QPen pen;
  pen.setColor(Qt::red);
  pen.setWidth(3);
  pen.setStyle(Qt::DashLine);
  painter.setPen(pen);

  int width = pixmap.width();
  int height = pixmap.height();

  if (width <= 0)
    return;

  const int GAP = 2;
  int pieceWidth = width / 5;

  // 绘制切割线（实际切割位置）
  for (int i = 1; i <= 4; ++i) {
    // 实际切割线位置 = i * pieceWidth - GAP/2
    // 用虚线表示间隔
    int x = i * pieceWidth;
    painter.drawLine(x, 0, x, height);

    // 绘制间隔标记（可选：用不同颜色标记间隔区域）
    QPen gapPen;
    gapPen.setColor(QColor(255, 200, 0, 100)); // 半透明橙色
    gapPen.setWidth(1);
    painter.setPen(gapPen);
    painter.drawLine(x - GAP, 0, x - GAP, height);
    painter.drawLine(x + GAP, 0, x + GAP, height);

    // 恢复红色虚线笔
    painter.setPen(pen);
  }

  painter.end();
}

void MainWindow::startProcess() {
  if (m_inputPath.isEmpty()) {
    QMessageBox::warning(this, "提示", "请先选择GIF文件");
    return;
  }

  updateUIState(true);

  m_processor->setInputPath(m_inputPath);
  m_processor->setOutputDir(m_outputDir);

  QMetaObject::invokeMethod(m_processor, "process", Qt::QueuedConnection);
}

void MainWindow::onProgressUpdated(int percent) {
  ui->progressBar->setValue(percent);
}

void MainWindow::onStatusMessage(const QString &message) {
  ui->statusLabel->setText(message);
}

void MainWindow::onFinished(const QStringList &outputFiles) {
  updateUIState(false);
  ui->progressBar->setValue(100);
  ui->statusLabel->setText("✅ 处理完成！文件已保存");

  QMessageBox::information(
      this, "完成",
      QString("🎉 成功生成 %1 个 GIF 文件！\n\n📁 保存在:\n%2")
          .arg(outputFiles.size())
          .arg(m_outputDir));
}

void MainWindow::onError(const QString &message) {
  updateUIState(false);
  ui->statusLabel->setText("❌ 处理失败: " + message);
  QMessageBox::critical(this, "错误", message);
}

void MainWindow::updateUIState(bool processing) {
  ui->fileButton->setEnabled(!processing);
  ui->outputButton->setEnabled(!processing);
  ui->startButton->setEnabled(!processing);

  if (processing) {
    ui->progressBar->setValue(0);
    ui->statusLabel->setText("⏳ 处理中...");
  } else {
    ui->startButton->setEnabled(!m_inputPath.isEmpty());
    ui->statusLabel->setText("✨ 就绪，等待开始");
  }
}

void MainWindow::onSizeWarning(const QString &message) {
  // 在状态栏显示警告
  ui->statusLabel->setText("⚠️ " + message.left(80));
  ui->statusLabel->setStyleSheet("color: #f0a020; font-weight: bold;");

  // 弹窗警告
  QMessageBox::warning(this, "文件大小超限", message);

  // 恢复样式
  ui->statusLabel->setStyleSheet("");
}