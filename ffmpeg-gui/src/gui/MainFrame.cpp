#include "MainFrame.h"

#include <fstream>
#include <algorithm>

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/clipbrd.h>
#include <wx/statbox.h>
#include <wx/filename.h>
#include <wx/gauge.h>

#include "command/CommandBuilder.h"
#include "core/ExtraArgsParser.h"
#include "core/JobValidator.h"

// ─── file-scope helpers ───────────────────────────────────────────────────────
namespace {

wxSizer* LabeledRow(wxWindow* parent, const wxString& label, wxWindow* ctrl)
{
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(new wxStaticText(parent, wxID_ANY, label),
             wxSizerFlags().CenterVertical().Border(wxRIGHT, 5));
    row->Add(ctrl, wxSizerFlags(1).Expand());
    return row;
}

const wxString kVideoContainers[] = { "mp4", "mkv", "webm", "avi", "mov" };
const wxString kAudioContainers[] = { "mp3", "wav", "ogg", "flac", "m4a", "opus" };
const int      kNVideoContainers  = 5;
const int      kNAudioContainers  = 6;

// ─── Presets ─────────────────────────────────────────────────────────────────
struct PresetDef {
    const char* name;
    const char* vCodec;
    const char* aCodec;
    const char* container;
    bool        audioOnly;
    bool        useCrf;
    int         crf;
    int         vBitrate;
    int         aBitrate;
    const char* resolution;
};

const PresetDef kPresets[] = {
    // name                     vCodec        aCodec         container  audioOnly useCrf crf vBit  aBit  resolution
    { "Custom",                 "",           "",            "",        false,    true,  23, 2000, 128,  "Original" },
    { "Web MP4 (H.264/AAC)",    "libx264",    "aac",         "mp4",     false,    true,  23, 2000, 128,  "Original" },
    { "Web MP4 720p",           "libx264",    "aac",         "mp4",     false,    true,  23, 2000, 128,  "1280x720" },
    { "HEVC/H.265 (MKV)",       "libx265",    "aac",         "mkv",     false,    true,  28, 2000, 128,  "Original" },
    { "Extract audio (MP3)",    "",           "libmp3lame",  "mp3",     true,     true,  23, 2000, 192,  "Original" },
    { "Extract audio (AAC)",    "",           "aac",         "m4a",     true,     true,  23, 2000, 160,  "Original" },
    { "Compress (small file)",  "libx264",    "aac",         "mp4",     false,    true,  32, 2000,  96,  "Original" },
    { "Stream copy",            "copy",       "copy",        "mkv",     false,    true,  23, 2000, 128,  "Original" },
};
const int kNPresets = 8;

} // namespace

// ─── ctor ────────────────────────────────────────────────────────────────────
MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "FFmpeg GUI",
              wxDefaultPosition, wxSize(780, 840)),
      m_pollTimer(this, ID_POLL_TIMER)
{
    BuildUI();
    UpdateCommandPreview();

    Bind(wxEVT_TIMER, &MainFrame::OnPollTimer, this, ID_POLL_TIMER);

    SetMinSize(wxSize(640, 640));
    Centre();
}

// ─── BuildUI ─────────────────────────────────────────────────────────────────
void MainFrame::BuildUI()
{
    auto* panel = new wxPanel(this);
    auto* main  = new wxBoxSizer(wxVERTICAL);

    const int B = 8;
    const auto F = [&](int prop = 0) {
        return wxSizerFlags(prop).Expand().Border(wxALL, B);
    };

    // ── Files ────────────────────────────────────────────────────────────────
    {
        auto* box  = new wxStaticBoxSizer(wxVERTICAL, panel, "Files");
        auto* grid = new wxFlexGridSizer(0, 3, 4, 6);
        grid->AddGrowableCol(1, 1);

        m_inputPath  = new wxTextCtrl(panel, wxID_ANY);
        m_outputPath = new wxTextCtrl(panel, wxID_ANY);
        m_ffmpegPath = new wxTextCtrl(panel, wxID_ANY, "ffmpeg");

        m_container = new wxChoice(panel, ID_CONTAINER,
                                   wxDefaultPosition, wxDefaultSize,
                                   kNVideoContainers, kVideoContainers);
        m_container->SetSelection(0);

        grid->Add(new wxStaticText(panel, wxID_ANY, "Input:"),
                  wxSizerFlags().CenterVertical());
        grid->Add(m_inputPath,  wxSizerFlags(1).Expand());
        grid->Add(new wxButton(panel, ID_INPUT_BROWSE, "Browse\xe2\x80\xa6"),
                  wxSizerFlags().CenterVertical());

        grid->Add(new wxStaticText(panel, wxID_ANY, "Output:"),
                  wxSizerFlags().CenterVertical());
        grid->Add(m_outputPath, wxSizerFlags(1).Expand());
        grid->Add(new wxButton(panel, ID_OUTPUT_BROWSE, "Browse\xe2\x80\xa6"),
                  wxSizerFlags().CenterVertical());

        grid->Add(new wxStaticText(panel, wxID_ANY, "Container:"),
                  wxSizerFlags().CenterVertical());
        grid->Add(m_container, wxSizerFlags().Border(wxRIGHT, 4));
        grid->AddStretchSpacer();

        grid->Add(new wxStaticText(panel, wxID_ANY, "ffmpeg:"),
                  wxSizerFlags().CenterVertical());
        grid->Add(m_ffmpegPath, wxSizerFlags(1).Expand());
        grid->Add(new wxButton(panel, ID_FFMPEG_BROWSE, "Browse\xe2\x80\xa6"),
                  wxSizerFlags().CenterVertical());

        box->Add(grid, F(1));
        main->Add(box, wxSizerFlags().Expand().Border(wxALL, B));
    }

    // ── Settings ─────────────────────────────────────────────────────────────
    {
        auto* box = new wxStaticBoxSizer(wxVERTICAL, panel, "Settings");

        // Row: presets
        {
            wxArrayString presetNames;
            for (int i = 0; i < kNPresets; ++i)
                presetNames.Add(kPresets[i].name);
            m_preset = new wxChoice(panel, ID_PRESET,
                                    wxDefaultPosition, wxDefaultSize,
                                    presetNames);
            m_preset->SetSelection(0);
            box->Add(LabeledRow(panel, "Preset:", m_preset), F());
        }

        // Row: codecs + audio-only
        {
            wxString vCodecs[] = { "libx264", "libx265", "libvpx-vp9",
                                   "libsvtav1", "copy" };
            wxString aCodecs[] = { "aac", "libmp3lame", "libopus",
                                   "flac", "copy" };
            m_videoCodec = new wxChoice(panel, ID_VIDEO_CODEC,
                                        wxDefaultPosition, wxDefaultSize,
                                        5, vCodecs);
            m_audioCodec = new wxChoice(panel, ID_AUDIO_CODEC,
                                        wxDefaultPosition, wxDefaultSize,
                                        5, aCodecs);
            m_audioOnly  = new wxCheckBox(panel, ID_AUDIO_ONLY, "Extract audio only");
            m_videoCodec->SetSelection(0);
            m_audioCodec->SetSelection(0);

            auto* row = new wxBoxSizer(wxHORIZONTAL);
            row->Add(LabeledRow(panel, "Video codec:", m_videoCodec),
                     wxSizerFlags(1).Expand().Border(wxRIGHT, 12));
            row->Add(LabeledRow(panel, "Audio codec:", m_audioCodec),
                     wxSizerFlags(1).Expand().Border(wxRIGHT, 12));
            row->Add(m_audioOnly, wxSizerFlags().CenterVertical());
            box->Add(row, F());
        }

        // Row: quality
        {
            m_crfRadio    = new wxRadioButton(panel, ID_CRF_RADIO, "CRF",
                                              wxDefaultPosition, wxDefaultSize,
                                              wxRB_GROUP);
            m_bitrateRadio= new wxRadioButton(panel, ID_BITRATE_RADIO, "Bitrate (kbps):");
            m_crfSpin     = new wxSpinCtrl(panel, ID_CRF_SPIN, "23",
                                           wxDefaultPosition, wxDefaultSize,
                                           wxSP_ARROW_KEYS, 0, 51, 23);
            m_bitrateSpin = new wxSpinCtrl(panel, ID_BITRATE_SPIN, "2000",
                                           wxDefaultPosition, wxDefaultSize,
                                           wxSP_ARROW_KEYS, 100, 100000, 2000);
            m_bitrateSpin->Disable();

            auto* row = new wxBoxSizer(wxHORIZONTAL);
            row->Add(m_crfRadio,     wxSizerFlags().CenterVertical().Border(wxRIGHT, 4));
            row->Add(m_crfSpin,      wxSizerFlags().CenterVertical().Border(wxRIGHT, 20));
            row->Add(m_bitrateRadio, wxSizerFlags().CenterVertical().Border(wxRIGHT, 4));
            row->Add(m_bitrateSpin,  wxSizerFlags().CenterVertical());
            box->Add(row, F());
        }

        // Row: resolution + audio bitrate
        {
            wxString resolutions[] = { "Original", "3840x2160", "1920x1080",
                                       "1280x720", "854x480",   "640x360" };
            m_resolution  = new wxChoice(panel, ID_RESOLUTION,
                                         wxDefaultPosition, wxDefaultSize,
                                         6, resolutions);
            m_resolution->SetSelection(0);

            m_audioBitSpin = new wxSpinCtrl(panel, ID_AUDIO_BITRATE_SPIN, "128",
                                            wxDefaultPosition, wxDefaultSize,
                                            wxSP_ARROW_KEYS, 32, 1024, 128);

            auto* row = new wxBoxSizer(wxHORIZONTAL);
            row->Add(LabeledRow(panel, "Resolution:", m_resolution),
                     wxSizerFlags(1).Expand().Border(wxRIGHT, 12));
            row->Add(LabeledRow(panel, "Audio bitrate (kbps):", m_audioBitSpin),
                     wxSizerFlags(1).Expand());
            box->Add(row, F());
        }

        // Row: trim
        {
            m_trimCheck    = new wxCheckBox(panel, ID_TRIM_CHECK, "Trim");
            m_trimStart    = new wxTextCtrl(panel, ID_TRIM_START, "",
                                            wxDefaultPosition, wxSize(88, -1));
            m_trimEndRadio = new wxRadioButton(panel, ID_TRIM_END_RADIO, "End time:",
                                               wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
            m_trimEnd      = new wxTextCtrl(panel, ID_TRIM_END,      "",
                                            wxDefaultPosition, wxSize(88, -1));
            m_trimDurRadio = new wxRadioButton(panel, ID_TRIM_DUR_RADIO, "Duration:");
            m_trimDuration = new wxTextCtrl(panel, ID_TRIM_DURATION, "",
                                            wxDefaultPosition, wxSize(88, -1));

            m_trimStart->Disable();
            m_trimEndRadio->Disable();
            m_trimEnd->Disable();
            m_trimDurRadio->Disable();
            m_trimDuration->Disable();
            m_trimEndRadio->SetValue(true);

            auto* row = new wxBoxSizer(wxHORIZONTAL);
            row->Add(m_trimCheck,   wxSizerFlags().CenterVertical().Border(wxRIGHT, 10));
            row->Add(LabeledRow(panel, "From:", m_trimStart),  wxSizerFlags().Border(wxRIGHT, 8));
            row->Add(m_trimEndRadio,wxSizerFlags().CenterVertical().Border(wxRIGHT, 4));
            row->Add(m_trimEnd,     wxSizerFlags().CenterVertical().Border(wxRIGHT, 12));
            row->Add(m_trimDurRadio,wxSizerFlags().CenterVertical().Border(wxRIGHT, 4));
            row->Add(m_trimDuration,wxSizerFlags().CenterVertical());
            box->Add(row, F());
        }

        // Row: extra args
        {
            m_extraArgs = new wxTextCtrl(panel, ID_EXTRA_ARGS, "",
                                         wxDefaultPosition, wxDefaultSize);
            box->Add(LabeledRow(panel, "Extra args:", m_extraArgs), F());
        }

        main->Add(box, wxSizerFlags().Expand().Border(wxALL, B));
    }

    // ── Command preview ───────────────────────────────────────────────────────
    {
        auto* box    = new wxStaticBoxSizer(wxVERTICAL, panel, "Generated command");
        m_cmdPreview = new wxTextCtrl(panel, wxID_ANY, wxEmptyString,
                                      wxDefaultPosition, wxSize(-1, 56),
                                      wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
        m_cmdPreview->SetBackgroundColour(panel->GetBackgroundColour());

        // Command row: preview + Copy + Export script
        auto* cmdRow = new wxBoxSizer(wxHORIZONTAL);
        cmdRow->Add(m_cmdPreview, wxSizerFlags(1).Expand().Border(wxRIGHT, 6));
        cmdRow->Add(new wxButton(panel, ID_COPY_CMD,      "Copy"),
                    wxSizerFlags().CenterVertical().Border(wxBOTTOM, 4));
        cmdRow->Add(new wxButton(panel, ID_EXPORT_SCRIPT, "Export script\xe2\x80\xa6"),
                    wxSizerFlags().CenterVertical().Border(wxLEFT, 4));
        box->Add(cmdRow, F(1));

        // History row
        m_historyList = new wxChoice(panel, ID_HISTORY_SEL);
        m_historyList->Append("Recent commands\xe2\x80\xa6");
        m_historyList->SetSelection(0);
        box->Add(LabeledRow(panel, "Recent:", m_historyList),
                 wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, B));

        main->Add(box, wxSizerFlags().Expand().Border(wxALL, B));
    }

    // ── Execute row ───────────────────────────────────────────────────────────
    {
        m_executeBtn  = new wxButton(panel, ID_EXECUTE, "Execute");
        m_stopBtn     = new wxButton(panel, ID_STOP,    "Stop");
        m_statusLabel = new wxStaticText(panel, wxID_ANY, "Idle",
                                         wxDefaultPosition, wxSize(200, -1));
        m_stopBtn->Disable();

        auto* row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(m_executeBtn,  wxSizerFlags().CenterVertical().Border(wxRIGHT, 8));
        row->Add(m_stopBtn,     wxSizerFlags().CenterVertical().Border(wxRIGHT, 16));
        row->Add(new wxStaticText(panel, wxID_ANY, "Status:"),
                 wxSizerFlags().CenterVertical().Border(wxRIGHT, 6));
        row->Add(m_statusLabel, wxSizerFlags().CenterVertical());
        main->Add(row, wxSizerFlags().Border(wxALL, B));
    }

    // ── Progress ─────────────────────────────────────────────────────────────
    {
        m_progressBar  = new wxGauge(panel, wxID_ANY, 1000);
        m_progressInfo = new wxStaticText(panel, wxID_ANY, wxEmptyString,
                                          wxDefaultPosition, wxDefaultSize,
                                          wxST_NO_AUTORESIZE);
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(m_progressBar,  wxSizerFlags(1).Expand().Border(wxRIGHT, 8));
        row->Add(m_progressInfo, wxSizerFlags(2).CenterVertical());
        main->Add(row, wxSizerFlags().Expand().Border(wxLEFT | wxRIGHT | wxBOTTOM, B));
    }

    // ── Log area ─────────────────────────────────────────────────────────────
    {
        auto* box = new wxStaticBoxSizer(wxVERTICAL, panel, "Log");
        m_logArea = new wxTextCtrl(panel, wxID_ANY, wxEmptyString,
                                   wxDefaultPosition, wxSize(-1, 140),
                                   wxTE_MULTILINE | wxTE_READONLY |
                                   wxTE_DONTWRAP  | wxHSCROLL);
        wxFont mono = wxFont(wxFontInfo(9).Family(wxFONTFAMILY_TELETYPE));
        m_logArea->SetFont(mono);
        box->Add(m_logArea, wxSizerFlags(1).Expand().Border(wxALL, 4));
        main->Add(box, wxSizerFlags(1).Expand().Border(wxALL, B));
    }

    panel->SetSizerAndFit(main);

    // ── Event bindings ────────────────────────────────────────────────────────
    Bind(wxEVT_BUTTON,      &MainFrame::OnInputBrowse,    this, ID_INPUT_BROWSE);
    Bind(wxEVT_BUTTON,      &MainFrame::OnOutputBrowse,   this, ID_OUTPUT_BROWSE);
    Bind(wxEVT_BUTTON,      &MainFrame::OnFfmpegBrowse,   this, ID_FFMPEG_BROWSE);
    Bind(wxEVT_CHOICE,      &MainFrame::OnContainerChange,this, ID_CONTAINER);
    Bind(wxEVT_CHOICE,      &MainFrame::OnPresetChange,   this, ID_PRESET);
    Bind(wxEVT_CHECKBOX,    &MainFrame::OnAudioOnly,      this, ID_AUDIO_ONLY);
    Bind(wxEVT_RADIOBUTTON, &MainFrame::OnQualityRadio,   this, ID_CRF_RADIO);
    Bind(wxEVT_RADIOBUTTON, &MainFrame::OnQualityRadio,   this, ID_BITRATE_RADIO);
    Bind(wxEVT_CHECKBOX,    &MainFrame::OnTrimCheck,      this, ID_TRIM_CHECK);
    Bind(wxEVT_RADIOBUTTON, &MainFrame::OnTrimMode,       this, ID_TRIM_END_RADIO);
    Bind(wxEVT_RADIOBUTTON, &MainFrame::OnTrimMode,       this, ID_TRIM_DUR_RADIO);
    Bind(wxEVT_BUTTON,      &MainFrame::OnCopyCommand,    this, ID_COPY_CMD);
    Bind(wxEVT_BUTTON,      &MainFrame::OnExportScript,   this, ID_EXPORT_SCRIPT);
    Bind(wxEVT_CHOICE,      &MainFrame::OnHistorySelect,  this, ID_HISTORY_SEL);
    Bind(wxEVT_BUTTON,      &MainFrame::OnExecute,        this, ID_EXECUTE);
    Bind(wxEVT_BUTTON,      &MainFrame::OnStop,           this, ID_STOP);

    for (int id : { ID_VIDEO_CODEC, ID_AUDIO_CODEC, ID_RESOLUTION })
        Bind(wxEVT_CHOICE, &MainFrame::OnAnyChange, this, id);

    for (int id : { ID_CRF_SPIN, ID_BITRATE_SPIN, ID_AUDIO_BITRATE_SPIN })
        Bind(wxEVT_SPINCTRL, [this](wxSpinEvent&){ UpdateCommandPreview(); }, id);

    for (int id : { ID_TRIM_START, ID_TRIM_END, ID_TRIM_DURATION })
        Bind(wxEVT_TEXT, &MainFrame::OnAnyChange, this, id);

    m_inputPath ->Bind(wxEVT_TEXT, [this](wxCommandEvent&){ UpdateCommandPreview(); });
    m_outputPath->Bind(wxEVT_TEXT, [this](wxCommandEvent&){ UpdateCommandPreview(); });
    m_ffmpegPath->Bind(wxEVT_TEXT, [this](wxCommandEvent&){ UpdateCommandPreview(); });
    m_extraArgs ->Bind(wxEVT_TEXT, [this](wxCommandEvent&){ UpdateCommandPreview(); });
}

// ─── Domain helpers ───────────────────────────────────────────────────────────
FFmpegJob MainFrame::BuildJobFromUI() const
{
    FFmpegJob job;

    job.inputFile  = m_inputPath ->GetValue().ToUTF8().data();
    job.outputFile = m_outputPath->GetValue().ToUTF8().data();
    job.outputContainer = m_container->GetStringSelection().ToUTF8().data();

    // ffmpeg binary path
    job.ffmpegPath = m_ffmpegPath->GetValue().Trim().ToUTF8().data();
    if (job.ffmpegPath.empty()) job.ffmpegPath = "ffmpeg";

    // ── Video ──
    wxString vSel = m_videoCodec->GetStringSelection();
    job.video.copy  = (vSel == "copy");
    job.video.codec = job.video.copy ? "" : vSel.ToUTF8().data();
    job.video.useCrf  = m_crfRadio->GetValue();
    job.video.crf     = m_crfSpin->GetValue();
    job.video.bitrate = m_bitrateSpin->GetValue();

    wxString res = m_resolution->GetStringSelection();
    job.video.resolution = (res == "Original") ? "" : res.ToUTF8().data();

    // ── Audio ──
    wxString aSel = m_audioCodec->GetStringSelection();
    job.audio.copy        = (aSel == "copy");
    job.audio.codec       = job.audio.copy ? "" : aSel.ToUTF8().data();
    job.audio.extractOnly = m_audioOnly->GetValue();
    job.audio.bitrate     = m_audioBitSpin->GetValue();

    // ── Trim ──
    job.trim.enabled   = m_trimCheck->GetValue();
    job.trim.startTime = m_trimStart->GetValue().ToUTF8().data();
    job.trim.endTime   = m_trimEndRadio->GetValue()
                         ? m_trimEnd->GetValue().ToUTF8().data() : "";
    job.trim.duration  = m_trimDurRadio->GetValue()
                         ? m_trimDuration->GetValue().ToUTF8().data() : "";

    // ── Extra args — delegate to ExtraArgsParser (handles quoted tokens) ──
    wxString extra = m_extraArgs->GetValue().Trim();
    if (!extra.empty()) {
        auto parsed = ExtraArgsParser::Parse(extra.ToUTF8().data());
        job.extraArgs = parsed.args;   // errors are caught in OnExecute
    }

    return job;
}

void MainFrame::UpdateCommandPreview()
{
    m_cmdPreview->SetValue(
        wxString::FromUTF8(CommandBuilder::BuildCommandString(BuildJobFromUI())));
}

void MainFrame::AppendLog(const wxString& text)
{
    m_logArea->AppendText(text);
}

void MainFrame::SetRunningState(bool running)
{
    m_executeBtn->Enable(!running);
    m_stopBtn->Enable(running);
    if (running) {
        m_statusLabel->SetLabel("Running...");
        m_statusLabel->SetForegroundColour(*wxBLUE);
        m_statusLabel->GetParent()->Layout();
    }
}

void MainFrame::UpdateStatusLabel(ProcessState state, int exitCode)
{
    wxString text;
    wxColour color;

    switch (state) {
        case ProcessState::Succeeded:
            text  = "Finished OK";
            color = wxColour(0, 140, 0);
            break;
        case ProcessState::Failed:
            text  = wxString::Format("Failed (exit %d)", exitCode);
            color = *wxRED;
            break;
        case ProcessState::Canceled:
            text  = "Canceled";
            color = wxColour(200, 100, 0);
            break;
        default:
            text  = "Idle";
            color = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
            break;
    }

    m_statusLabel->SetLabel(text);
    m_statusLabel->SetForegroundColour(color);
    m_statusLabel->GetParent()->Layout();
}

void MainFrame::UpdateProgressUI(const ProgressInfo& info)
{
    if (info.percent >= 0.0)
        m_progressBar->SetValue(static_cast<int>(info.percent * 10.0));

    wxString label;
    if (!info.timeStr.empty()) {
        label = wxString::FromUTF8(info.timeStr);
        if (m_progressParser.HasDuration()) {
            double total = m_progressParser.GetTotalSec();
            int h  = static_cast<int>(total / 3600);
            int m  = static_cast<int>((total - h * 3600) / 60);
            double s = total - h * 3600 - m * 60;
            label += wxString::Format(" / %02d:%02d:%05.2f", h, m, s);
        }
    }
    if (info.fps   > 0.f) label += wxString::Format("   fps: %.0f",  info.fps);
    if (info.speed > 0.f) label += wxString::Format("   speed: %.2fx", info.speed);
    if (info.percent >= 0.0)
        label += wxString::Format("   (%.1f%%)", info.percent);

    m_progressInfo->SetLabel(label);
}

void MainFrame::ResetProgress()
{
    m_progressParser.Reset();
    m_progressBar->SetValue(0);
    m_progressInfo->SetLabel(wxEmptyString);
}

void MainFrame::AddToHistory(const std::string& cmd)
{
    // Move to front if duplicate exists
    auto it = std::find(m_history.begin(), m_history.end(), cmd);
    if (it != m_history.end()) m_history.erase(it);
    m_history.insert(m_history.begin(), cmd);
    if (static_cast<int>(m_history.size()) > kMaxHistory)
        m_history.pop_back();

    // Rebuild dropdown (index 0 = placeholder)
    m_historyList->Clear();
    m_historyList->Append("Recent commands\xe2\x80\xa6");
    for (const auto& c : m_history) {
        wxString s = wxString::FromUTF8(c);
        if (s.length() > 90) s = s.Left(87) + "...";
        m_historyList->Append(s);
    }
    m_historyList->SetSelection(0);
}

// ─── Event handlers ───────────────────────────────────────────────────────────
void MainFrame::OnInputBrowse(wxCommandEvent&)
{
    wxFileDialog dlg(this, "Select input file", wxEmptyString, wxEmptyString,
                     "Video/audio files (*.mp4;*.mkv;*.avi;*.mov;*.webm;*.mp3;*.wav;*.ogg)|"
                     "*.mp4;*.mkv;*.avi;*.mov;*.webm;*.mp3;*.wav;*.ogg|All files (*)|*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK)
        m_inputPath->SetValue(dlg.GetPath());
}

void MainFrame::OnOutputBrowse(wxCommandEvent&)
{
    wxString container = m_container->GetStringSelection();
    wxString filter;
    if (!container.empty())
        filter = container.Upper() + " files (*." + container + ")|*." + container + "|";
    filter += "All files (*)|*";

    wxFileDialog dlg(this, "Save output as", wxEmptyString, wxEmptyString,
                     filter, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_OK) {
        wxFileName fn(dlg.GetPath());
        if (!container.empty()) fn.SetExt(container);
        m_outputPath->ChangeValue(fn.GetFullPath());
        UpdateCommandPreview();
    }
}

void MainFrame::OnFfmpegBrowse(wxCommandEvent&)
{
    wxFileDialog dlg(this, "Select ffmpeg binary", wxEmptyString, wxEmptyString,
                     "All files (*)|*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        m_ffmpegPath->ChangeValue(dlg.GetPath());
        UpdateCommandPreview();
    }
}

void MainFrame::OnContainerChange(wxCommandEvent&)
{
    wxString path = m_outputPath->GetValue().Trim();
    if (!path.empty()) {
        wxFileName fn(path);
        wxString ext = m_container->GetStringSelection();
        if (!ext.empty()) {
            fn.SetExt(ext);
            m_outputPath->ChangeValue(fn.GetFullPath());
        }
    }
    UpdateCommandPreview();
}

void MainFrame::OnPresetChange(wxCommandEvent&)
{
    int idx = m_preset->GetSelection();
    if (idx <= 0) return;  // "Custom" = no-op

    const PresetDef& p = kPresets[idx];

    // Set audio-only first (repopulates container dropdown)
    m_audioOnly->SetValue(p.audioOnly);
    wxCommandEvent dummy;
    OnAudioOnly(dummy);

    // Set container after repopulation
    if (p.container[0] != '\0') {
        int ci = m_container->FindString(p.container);
        if (ci != wxNOT_FOUND) {
            m_container->SetSelection(ci);
            OnContainerChange(dummy);
        }
    }

    // Video codec
    if (!p.audioOnly && p.vCodec[0] != '\0') {
        int vi = m_videoCodec->FindString(p.vCodec);
        if (vi != wxNOT_FOUND) m_videoCodec->SetSelection(vi);
    }

    // Audio codec
    if (p.aCodec[0] != '\0') {
        int ai = m_audioCodec->FindString(p.aCodec);
        if (ai != wxNOT_FOUND) m_audioCodec->SetSelection(ai);
    }

    // Quality
    m_crfRadio->SetValue(p.useCrf);
    m_bitrateRadio->SetValue(!p.useCrf);
    m_crfSpin->SetValue(p.crf);
    m_bitrateSpin->SetValue(p.vBitrate);
    m_crfSpin->Enable(!p.audioOnly && p.useCrf);
    m_bitrateSpin->Enable(!p.audioOnly && !p.useCrf);

    m_audioBitSpin->SetValue(p.aBitrate);

    int ri = m_resolution->FindString(p.resolution);
    if (ri != wxNOT_FOUND) m_resolution->SetSelection(ri);

    UpdateCommandPreview();
}

void MainFrame::OnAudioOnly(wxCommandEvent&)
{
    bool audioOnly = m_audioOnly->GetValue();

    m_videoCodec->Enable(!audioOnly);
    m_crfRadio->Enable(!audioOnly);
    m_bitrateRadio->Enable(!audioOnly);
    m_crfSpin->Enable(!audioOnly && m_crfRadio->GetValue());
    m_bitrateSpin->Enable(!audioOnly && m_bitrateRadio->GetValue());
    m_resolution->Enable(!audioOnly);

    int prev = m_container->GetSelection();
    m_container->Clear();
    if (audioOnly) {
        for (int i = 0; i < kNAudioContainers; ++i)
            m_container->Append(kAudioContainers[i]);
    } else {
        for (int i = 0; i < kNVideoContainers; ++i)
            m_container->Append(kVideoContainers[i]);
    }
    m_container->SetSelection(0);
    (void)prev;

    wxCommandEvent dummy;
    OnContainerChange(dummy);
}

void MainFrame::OnQualityRadio(wxCommandEvent&)
{
    bool crf = m_crfRadio->GetValue();
    m_crfSpin->Enable(crf);
    m_bitrateSpin->Enable(!crf);
    UpdateCommandPreview();
}

void MainFrame::OnTrimCheck(wxCommandEvent&)
{
    bool on = m_trimCheck->GetValue();
    m_trimStart->Enable(on);
    m_trimEndRadio->Enable(on);
    m_trimDurRadio->Enable(on);
    m_trimEnd->Enable(on && m_trimEndRadio->GetValue());
    m_trimDuration->Enable(on && m_trimDurRadio->GetValue());
    UpdateCommandPreview();
}

void MainFrame::OnTrimMode(wxCommandEvent&)
{
    bool endMode = m_trimEndRadio->GetValue();
    m_trimEnd->Enable(endMode);
    m_trimDuration->Enable(!endMode);
    UpdateCommandPreview();
}

void MainFrame::OnCopyCommand(wxCommandEvent&)
{
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(m_cmdPreview->GetValue()));
        wxTheClipboard->Close();
    }
}

void MainFrame::OnExportScript(wxCommandEvent&)
{
    wxString cmd = m_cmdPreview->GetValue().Trim();
    if (cmd.empty()) return;

#ifdef _WIN32
    const wxString filter  = "Batch files (*.bat)|*.bat|All files (*)|*";
    const wxString defName = "convert.bat";
    const bool     isBat   = true;
#else
    const wxString filter  = "Shell scripts (*.sh)|*.sh|All files (*)|*";
    const wxString defName = "convert.sh";
    const bool     isBat   = false;
#endif

    wxFileDialog dlg(this, "Export script", wxEmptyString, defName,
                     filter, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) return;

    std::ofstream f(dlg.GetPath().ToUTF8().data());
    if (!f) {
        wxMessageBox("Could not write file.", "Error", wxOK | wxICON_ERROR, this);
        return;
    }
    if (!isBat) f << "#!/bin/sh\n";
    f << cmd.ToUTF8() << "\n";
}

void MainFrame::OnHistorySelect(wxCommandEvent&)
{
    int sel = m_historyList->GetSelection();
    if (sel <= 0 || sel > static_cast<int>(m_history.size())) return;

    wxString cmd = wxString::FromUTF8(m_history[sel - 1]);
    if (wxTheClipboard->Open()) {
        wxTheClipboard->SetData(new wxTextDataObject(cmd));
        wxTheClipboard->Close();
        m_statusLabel->SetLabel("Command copied!");
        m_statusLabel->SetForegroundColour(wxColour(0, 100, 180));
        m_statusLabel->GetParent()->Layout();
    }
    m_historyList->SetSelection(0);
}

void MainFrame::OnExecute(wxCommandEvent&)
{
    // Validate extra args before building the full job
    wxString rawExtra = m_extraArgs->GetValue().Trim();
    if (!rawExtra.empty()) {
        auto pr = ExtraArgsParser::Parse(rawExtra.ToUTF8().data());
        if (!pr.IsValid()) {
            wxString msg;
            for (const auto& e : pr.errors)
                msg += wxString::FromUTF8(e) + "\n";
            wxMessageBox(msg.Trim(), "Invalid extra args",
                         wxOK | wxICON_ERROR, this);
            return;
        }
    }

    FFmpegJob job = BuildJobFromUI();

    if (!FFmpegProcess::CheckAvailable(job.ffmpegPath)) {
        wxMessageBox(
            "ffmpeg not found.\n\n"
            "Make sure ffmpeg is installed and available in your PATH,\n"
            "or set a custom path in the ffmpeg field.",
            "ffmpeg not available", wxOK | wxICON_ERROR, this);
        return;
    }

    auto result = JobValidator::Validate(job);

    if (!result.IsValid()) {
        wxString msg;
        for (const auto& e : result.errors)
            msg += "\u2022 " + wxString::FromUTF8(e) + "\n";
        wxMessageBox(msg.Trim(), "Cannot execute \xe2\x80\x93 validation errors",
                     wxOK | wxICON_WARNING, this);
        return;
    }

    if (!result.warnings.empty()) {
        wxString msg;
        for (const auto& w : result.warnings)
            msg += "\u2022 " + wxString::FromUTF8(w) + "\n";
        msg += "\nProceed anyway?";
        if (wxMessageBox(msg.Trim(), "Warnings", wxYES_NO | wxICON_WARNING, this) != wxYES)
            return;
    }

    auto args = CommandBuilder::BuildArguments(job);

    m_logArea->Clear();
    ResetProgress();
    AppendLog("$ " + m_cmdPreview->GetValue() + "\n\n");

    m_process = std::make_unique<FFmpegProcess>();
    if (!m_process->Start(args, job.ffmpegPath)) {
        wxMessageBox("Failed to start ffmpeg process.",
                     "Process error", wxOK | wxICON_ERROR, this);
        m_process.reset();
        return;
    }

    AddToHistory(m_cmdPreview->GetValue().ToUTF8().data());

    SetRunningState(true);
    m_pollTimer.Start(100);
}

void MainFrame::OnStop(wxCommandEvent&)
{
    if (m_process && m_process->IsRunning()) {
        m_process->Kill();
        AppendLog("\n[Canceled by user]\n");
    }
}

void MainFrame::OnPollTimer(wxTimerEvent&)
{
    if (!m_process) { m_pollTimer.Stop(); return; }

    if (m_process->NeedsForceKill())
        m_process->ForceKill();

    std::string raw = m_process->PollOutput();
    if (!raw.empty()) {
        ParseResult pr = m_progressParser.Feed(raw);
        if (!pr.logText.empty())
            AppendLog(wxString::FromUTF8(pr.logText));
        if (pr.progressUpdated)
            UpdateProgressUI(pr.progress);
    }

    if (!m_process->IsRunning()) {
        m_pollTimer.Stop();

        raw = m_process->PollOutput();
        if (!raw.empty()) {
            ParseResult pr = m_progressParser.Feed(raw);
            if (!pr.logText.empty())
                AppendLog(wxString::FromUTF8(pr.logText));
            if (pr.progressUpdated)
                UpdateProgressUI(pr.progress);
        }

        ProcessState state    = m_process->GetState();
        int          exitCode = m_process->GetExitCode();

        switch (state) {
            case ProcessState::Succeeded:
                AppendLog("\n[Finished successfully]\n");
                m_progressBar->SetValue(1000);
                break;
            case ProcessState::Failed:
                AppendLog(wxString::Format("\n[Failed \xe2\x80\x93 exit code %d]\n", exitCode));
                break;
            case ProcessState::Canceled:
                AppendLog("\n[Canceled]\n");
                break;
            default:
                break;
        }

        UpdateStatusLabel(state, exitCode);
        SetRunningState(false);
        m_process.reset();
    }
}

void MainFrame::OnAnyChange(wxCommandEvent&)
{
    UpdateCommandPreview();
}
