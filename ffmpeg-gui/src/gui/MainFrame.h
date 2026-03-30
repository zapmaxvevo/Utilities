#pragma once
#include <memory>
#include <vector>
#include <string>
#include <wx/frame.h>
#include <wx/timer.h>

#include "process/FFmpegProcess.h"
#include "process/ProgressParser.h"
#include "core/FFmpegJob.h"

// Forward-declare widgets
class wxTextCtrl;
class wxChoice;
class wxRadioButton;
class wxSpinCtrl;
class wxCheckBox;
class wxButton;
class wxStaticText;
class wxGauge;
class wxTimerEvent;
class wxCommandEvent;

// ─── Widget IDs ──────────────────────────────────────────────────────────────
enum : int {
    ID_INPUT_BROWSE      = wxID_HIGHEST + 1,
    ID_OUTPUT_BROWSE,
    ID_FFMPEG_BROWSE,
    ID_CONTAINER,
    ID_PRESET,
    ID_VIDEO_CODEC,
    ID_AUDIO_CODEC,
    ID_AUDIO_ONLY,
    ID_CRF_RADIO,
    ID_BITRATE_RADIO,
    ID_CRF_SPIN,
    ID_BITRATE_SPIN,
    ID_AUDIO_BITRATE_SPIN,
    ID_RESOLUTION,
    ID_TRIM_CHECK,
    ID_TRIM_START,
    ID_TRIM_END_RADIO,
    ID_TRIM_END,
    ID_TRIM_DUR_RADIO,
    ID_TRIM_DURATION,
    ID_EXTRA_ARGS,
    ID_COPY_CMD,
    ID_EXPORT_SCRIPT,
    ID_HISTORY_SEL,
    ID_EXECUTE,
    ID_STOP,
    ID_POLL_TIMER,
};

// ─── MainFrame ───────────────────────────────────────────────────────────────
class MainFrame : public wxFrame
{
public:
    MainFrame();

private:
    void BuildUI();

    // ── Domain helpers ──
    FFmpegJob BuildJobFromUI() const;
    void      UpdateCommandPreview();
    void      AppendLog(const wxString& text);
    void      SetRunningState(bool running);
    void      UpdateStatusLabel(ProcessState state, int exitCode = 0);
    void      UpdateProgressUI(const ProgressInfo& info);
    void      ResetProgress();
    void      AddToHistory(const std::string& cmd);

    // ── Event handlers ──
    void OnInputBrowse      (wxCommandEvent&);
    void OnOutputBrowse     (wxCommandEvent&);
    void OnFfmpegBrowse     (wxCommandEvent&);
    void OnContainerChange  (wxCommandEvent&);
    void OnPresetChange     (wxCommandEvent&);
    void OnAudioOnly        (wxCommandEvent&);
    void OnQualityRadio     (wxCommandEvent&);
    void OnTrimCheck        (wxCommandEvent&);
    void OnTrimMode         (wxCommandEvent&);
    void OnCopyCommand      (wxCommandEvent&);
    void OnExportScript     (wxCommandEvent&);
    void OnHistorySelect    (wxCommandEvent&);
    void OnExecute          (wxCommandEvent&);
    void OnStop             (wxCommandEvent&);
    void OnPollTimer        (wxTimerEvent&);
    void OnAnyChange        (wxCommandEvent&);

    // ── Widgets ──
    // Files / tools
    wxTextCtrl*    m_inputPath     = nullptr;
    wxTextCtrl*    m_outputPath    = nullptr;
    wxChoice*      m_container     = nullptr;
    wxTextCtrl*    m_ffmpegPath    = nullptr;
    // Presets
    wxChoice*      m_preset        = nullptr;
    // Codecs
    wxChoice*      m_videoCodec    = nullptr;
    wxChoice*      m_audioCodec    = nullptr;
    wxCheckBox*    m_audioOnly     = nullptr;
    // Quality
    wxRadioButton* m_crfRadio      = nullptr;
    wxRadioButton* m_bitrateRadio  = nullptr;
    wxSpinCtrl*    m_crfSpin       = nullptr;
    wxSpinCtrl*    m_bitrateSpin   = nullptr;
    wxSpinCtrl*    m_audioBitSpin  = nullptr;
    // Resolution / trim / extra args
    wxChoice*      m_resolution    = nullptr;
    wxCheckBox*    m_trimCheck     = nullptr;
    wxTextCtrl*    m_trimStart     = nullptr;
    wxRadioButton* m_trimEndRadio  = nullptr;
    wxTextCtrl*    m_trimEnd       = nullptr;
    wxRadioButton* m_trimDurRadio  = nullptr;
    wxTextCtrl*    m_trimDuration  = nullptr;
    wxTextCtrl*    m_extraArgs     = nullptr;
    // Command + history + controls
    wxTextCtrl*    m_cmdPreview    = nullptr;
    wxChoice*      m_historyList   = nullptr;
    wxButton*      m_executeBtn    = nullptr;
    wxButton*      m_stopBtn       = nullptr;
    wxStaticText*  m_statusLabel   = nullptr;
    // Progress
    wxGauge*       m_progressBar   = nullptr;
    wxStaticText*  m_progressInfo  = nullptr;
    wxTextCtrl*    m_logArea       = nullptr;

    // ── Process, parser, timer & history ──
    std::unique_ptr<FFmpegProcess> m_process;
    ProgressParser                 m_progressParser;
    wxTimer                        m_pollTimer;
    std::vector<std::string>       m_history;
    static constexpr int           kMaxHistory = 20;
};
