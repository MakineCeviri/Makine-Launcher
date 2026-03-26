#pragma once

#include <QObject>
#include <QRect>
#include <QTimer>
#include <QFuture>
#include <QLoggingCategory>
#include <atomic>

Q_DECLARE_LOGGING_CATEGORY(lcOcr)

class PluginManager;

/**
 * Manages the real-time OCR loop on a background thread.
 * Timer fires → QtConcurrent::run → plugin OCR call → result back to UI thread.
 * Zero UI thread blocking.
 */
class OcrController : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool ocrActive READ ocrActive NOTIFY ocrActiveChanged)
    Q_PROPERTY(QRect captureRegion READ captureRegion WRITE setCaptureRegion NOTIFY captureRegionChanged)
    Q_PROPERTY(QString lastTranslation READ lastTranslation NOTIFY translationReady)
    Q_PROPERTY(QString lastOcrText READ lastOcrText NOTIFY ocrTextReady)
    Q_PROPERTY(int intervalMs READ intervalMs WRITE setIntervalMs NOTIFY intervalMsChanged)
    Q_PROPERTY(bool regionSelecting READ regionSelecting WRITE setRegionSelecting NOTIFY regionSelectingChanged)
    Q_PROPERTY(bool overlayVisible READ overlayVisible WRITE setOverlayVisible NOTIFY overlayVisibleChanged)
    Q_PROPERTY(bool processing READ processing NOTIFY processingChanged)

public:
    explicit OcrController(PluginManager* pm, QObject* parent = nullptr);

    bool ocrActive() const { return m_timer.isActive(); }
    QRect captureRegion() const { return m_region; }
    QString lastTranslation() const { return m_lastTranslation; }
    QString lastOcrText() const { return m_lastOcrText; }
    int intervalMs() const { return m_intervalMs; }
    bool regionSelecting() const { return m_regionSelecting; }
    bool overlayVisible() const { return m_overlayVisible; }
    bool processing() const { return m_processing.load(std::memory_order_relaxed); }

    Q_INVOKABLE void startOcr();
    Q_INVOKABLE void stopOcr();
    Q_INVOKABLE void toggleOcr();
    Q_INVOKABLE void setCaptureRegion(const QRect& rect);
    Q_INVOKABLE void setRegion(int x, int y, int w, int h);
    Q_INVOKABLE void setIntervalMs(int ms);
    Q_INVOKABLE void setRegionSelecting(bool selecting);
    Q_INVOKABLE void setOverlayVisible(bool visible);

signals:
    void ocrActiveChanged();
    void captureRegionChanged();
    void translationReady(const QString& text);
    void ocrTextReady(const QString& text);
    void intervalMsChanged();
    void regionSelectingChanged();
    void overlayVisibleChanged();
    void ocrError(const QString& error);
    void processingChanged();

private slots:
    void onTimerTick();
    void onOcrResult(const QString& result);

private:
    PluginManager* m_pluginManager;
    QTimer m_timer;
    QRect m_region;
    QString m_lastTranslation;
    QString m_lastOcrText;
    int m_intervalMs = 2000;
    bool m_regionSelecting = false;
    bool m_overlayVisible = false;
    std::atomic<bool> m_processing{false};

    // Adaptive timer — fast when changes detected, slow when stable
    int m_fastIntervalMs = 200;   // Active change scanning
    int m_noChangeCount = 0;      // Consecutive ticks with no change
    static constexpr int kSlowdownThreshold = 5; // After 5 unchanged ticks, slow down

    static constexpr const char* kPluginId = "com.makineceviri.live";
};
