#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMovie>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>

class GifProcessor;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private slots:
  void selectInputFile();
  void selectOutputDir();
  void startProcess();
  void onProgressUpdated(int percent);
  void onStatusMessage(const QString &message);
  void onFinished(const QStringList &outputFiles);
  void onError(const QString &message);
  void onSizeWarning(const QString &message); // 新增：处理大小警告
  void updatePreview();

private:
  void setupUI();
  void updateUIState(bool processing);
  void loadPreview();
  void drawCutLines(QPixmap &pixmap);
  void resizePreview();

  Ui::MainWindow *ui;

  QString m_inputPath;
  QString m_outputDir;

  QThread m_workerThread;
  GifProcessor *m_processor;

  // 预览相关
  QMovie *m_movie;
  bool m_isPreviewLoaded;
  QSize m_originalSize;
};

#endif // MAINWINDOW_H