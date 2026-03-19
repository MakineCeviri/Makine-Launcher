#include "ocrcontroller.h"
#include "pluginmanager.h"

#include <QtConcurrent>

Q_LOGGING_CATEGORY(lcOcr, "makine.ocr")

OcrController::OcrController(PluginManager* pm, QObject* parent)
    : QObject(parent), m_pluginManager(pm)
{
    connect(&m_timer, &QTimer::timeout, this, &OcrController::onTimerTick);
}

void OcrController::startOcr()
{
    if (m_region.isEmpty()) {
        emit ocrError(QStringLiteral("No capture region selected"));
        return;
    }
    if (!m_pluginManager || !m_pluginManager->isPluginLoaded(QString::fromLatin1(kPluginId))) {
        emit ocrError(QStringLiteral("OCR plugin not loaded"));
        return;
    }

    // Start with fast interval for responsive first scan
    m_noChangeCount = 0;
    m_timer.start(m_fastIntervalMs);
    emit ocrActiveChanged();
    qCInfo(lcOcr) << "OCR started — fast:" << m_fastIntervalMs
                  << "ms, slow:" << m_intervalMs << "ms, region:" << m_region;
}

void OcrController::stopOcr()
{
    m_timer.stop();
    m_noChangeCount = 0;
    emit ocrActiveChanged();
    qCInfo(lcOcr) << "OCR stopped";
}

void OcrController::toggleOcr()
{
    if (m_timer.isActive())
        stopOcr();
    else
        startOcr();
}

void OcrController::setCaptureRegion(const QRect& rect)
{
    if (m_region == rect) return;
    m_region = rect;
    emit captureRegionChanged();
    qCInfo(lcOcr) << "Capture region set:" << m_region;
}

void OcrController::setRegion(int x, int y, int w, int h)
{
    setCaptureRegion(QRect(x, y, w, h));
}

void OcrController::setIntervalMs(int ms)
{
    if (ms < 100) ms = 100;
    if (ms > 10000) ms = 10000;
    if (m_intervalMs == ms) return;
    m_intervalMs = ms;
    emit intervalMsChanged();
}

void OcrController::setRegionSelecting(bool selecting)
{
    if (m_regionSelecting == selecting) return;
    m_regionSelecting = selecting;
    emit regionSelectingChanged();
}

void OcrController::setOverlayVisible(bool visible)
{
    if (m_overlayVisible == visible) return;
    m_overlayVisible = visible;
    emit overlayVisibleChanged();
}

void OcrController::onTimerTick()
{
    // Skip if previous OCR still running (non-blocking guard)
    if (m_processing.load(std::memory_order_relaxed)) return;
    if (!m_pluginManager || m_region.isEmpty()) return;

    m_processing.store(true, std::memory_order_relaxed);
    emit processingChanged();

    const int rx = m_region.x();
    const int ry = m_region.y();
    const int rw = m_region.width();
    const int rh = m_region.height();
    PluginManager* pm = m_pluginManager;

    // Run OCR+translate on background thread — zero UI blocking
    auto future = QtConcurrent::run([pm, rx, ry, rw, rh]() -> QString {
        return pm->callPluginOcr(
            QStringLiteral("com.makineceviri.live"), rx, ry, rw, rh);
    });

    auto* watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        onOcrResult(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}

void OcrController::onOcrResult(const QString& result)
{
    m_processing.store(false, std::memory_order_relaxed);
    emit processingChanged();

    if (result.isEmpty()) return;

    if (result.startsWith(QStringLiteral("CAPTURE_FAILED"))) {
        emit ocrError(result);
        return;
    }

    // Dual-phase: fetch raw OCR text (before translation) from plugin
    if (m_pluginManager) {
        QString ocrText = m_pluginManager->getPluginLastOcrText(
            QString::fromLatin1(kPluginId));
        if (!ocrText.isEmpty() && ocrText != m_lastOcrText) {
            m_lastOcrText = ocrText;
            emit ocrTextReady(m_lastOcrText);
        }
    }

    // Adaptive timer — speed up when changes detected, slow down when stable
    if (result != m_lastTranslation) {
        // Text changed — switch to fast scanning
        m_lastTranslation = result;
        m_noChangeCount = 0;
        if (m_timer.interval() != m_fastIntervalMs)
            m_timer.setInterval(m_fastIntervalMs);
        emit translationReady(m_lastTranslation);
    } else {
        // No change — count towards slowdown
        m_noChangeCount++;
        if (m_noChangeCount >= kSlowdownThreshold && m_timer.interval() != m_intervalMs) {
            m_timer.setInterval(m_intervalMs);
            qCDebug(lcOcr) << "Adaptive: switching to slow interval" << m_intervalMs << "ms";
        }
    }
}
