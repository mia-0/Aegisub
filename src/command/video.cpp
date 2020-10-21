// Copyright (c) 2005-2010, Niels Martin Hansen
// Copyright (c) 2005-2010, Rodrigo Braz Monteiro
// Copyright (c) 2010, Amar Takhar
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//	 this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//	 this list of conditions and the following disclaimer in the documentation
//	 and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//	 may be used to endorse or promote products derived from this software
//	 without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

#include "command.h"

#include "../ass_dialogue.h"
#include "../ass_file.h"
#include "../ass_style.h"
#include "../async_video_provider.h"
#include "../compat.h"
#include "../dialog_detached_video.h"
#include "../dialog_manager.h"
#include "../dialogs.h"
#include "../format.h"
#include "../frame_main.h"
#include "../include/aegisub/context.h"
#include "../include/aegisub/subtitles_provider.h"
#include "../libresrc/libresrc.h"
#include "../options.h"
#include "../project.h"
#include "../selection_controller.h"
#include "../text_selection_controller.h"
#include "../utils.h"
#include "../video_controller.h"
#include "../video_display.h"
#include "../video_frame.h"

#include <libaegisub/ass/time.h>
#include <libaegisub/fs.h>
#include <libaegisub/path.h>
#include <libaegisub/make_unique.h>
#include <libaegisub/of_type_adaptor.h>
#include <libaegisub/util.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/range/adaptor/sliced.hpp>
#include <wx/msgdlg.h>
#include <wx/textdlg.h>

namespace {
	using cmd::Command;
	using namespace boost::adaptors;

struct validator_video_loaded : public Command {
	CMD_TYPE(COMMAND_VALIDATE)
	bool Validate(const agi::Context *c) override {
		return !!c->project->VideoProvider();
	}
};

struct validator_video_attached : public Command {
	CMD_TYPE(COMMAND_VALIDATE)
	bool Validate(const agi::Context *c) override {
		return !!c->project->VideoProvider() && !c->dialog->Get<DialogDetachedVideo>();
	}
};

struct video_aspect_cinematic final : public validator_video_loaded {
	CMD_NAME("video/aspect/cinematic")
	STR_MENU("&Cinematic (2.35)")
	STR_DISP("Cinematic (2.35)")
	STR_HELP("Force video to 2.35 aspect ratio")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	bool IsActive(const agi::Context *c) override {
		return c->videoController->GetAspectRatioType() == AspectRatio::Cinematic;
	}

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		c->videoController->SetAspectRatio(AspectRatio::Cinematic);
		c->frame->SetDisplayMode(1,-1);
	}
};

struct video_aspect_custom final : public validator_video_loaded {
	CMD_NAME("video/aspect/custom")
	STR_MENU("C&ustom...")
	STR_DISP("Custom")
	STR_HELP("Force video to a custom aspect ratio")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	bool IsActive(const agi::Context *c) override {
		return c->videoController->GetAspectRatioType() == AspectRatio::Custom;
	}

	void operator()(agi::Context *c) override {
		c->videoController->Stop();

		std::string value = from_wx(wxGetTextFromUser(
			_("Enter aspect ratio in either:\n  decimal (e.g. 2.35)\n  fractional (e.g. 16:9)\n  specific resolution (e.g. 853x480)"),
			_("Enter aspect ratio"),
			std::to_wstring(c->videoController->GetAspectRatioValue())));
		if (value.empty()) return;

		double numval = 0;
		if (agi::util::try_parse(value, &numval)) {
			//Nothing to see here, move along
		}
		else {
			std::vector<std::string> chunks;
			split(chunks, value, boost::is_any_of(":/xX"));
			if (chunks.size() == 2) {
				double num, den;
				if (agi::util::try_parse(chunks[0], &num) && agi::util::try_parse(chunks[1], &den))
					numval = num / den;
			}
		}

		if (numval < 0.5 || numval > 5.0)
			wxMessageBox(_("Invalid value! Aspect ratio must be between 0.5 and 5.0."),_("Invalid Aspect Ratio"),wxOK | wxICON_ERROR | wxCENTER);
		else {
			c->videoController->SetAspectRatio(numval);
			c->frame->SetDisplayMode(1,-1);
		}
	}
};

struct video_aspect_default final : public validator_video_loaded {
	CMD_NAME("video/aspect/default")
	STR_MENU("&Default")
	STR_DISP("Default")
	STR_HELP("Use video's original aspect ratio")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	bool IsActive(const agi::Context *c) override {
		return c->videoController->GetAspectRatioType() == AspectRatio::Default;
	}

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		c->videoController->SetAspectRatio(AspectRatio::Default);
		c->frame->SetDisplayMode(1,-1);
	}
};

struct video_aspect_full final : public validator_video_loaded {
	CMD_NAME("video/aspect/full")
	STR_MENU("&Fullscreen (4:3)")
	STR_DISP("Fullscreen (4:3)")
	STR_HELP("Force video to 4:3 aspect ratio")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	bool IsActive(const agi::Context *c) override {
		return c->videoController->GetAspectRatioType() == AspectRatio::Fullscreen;
	}

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		c->videoController->SetAspectRatio(AspectRatio::Fullscreen);
		c->frame->SetDisplayMode(1,-1);
	}
};

struct video_aspect_wide final : public validator_video_loaded {
	CMD_NAME("video/aspect/wide")
	STR_MENU("&Widescreen (16:9)")
	STR_DISP("Widescreen (16:9)")
	STR_HELP("Force video to 16:9 aspect ratio")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	bool IsActive(const agi::Context *c) override {
		return c->videoController->GetAspectRatioType() == AspectRatio::Widescreen;
	}

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		c->videoController->SetAspectRatio(AspectRatio::Widescreen);
		c->frame->SetDisplayMode(1,-1);
	}
};

struct video_close final : public validator_video_loaded {
	CMD_NAME("video/close")
	CMD_ICON(close_video_menu)
	STR_MENU("&Close Video")
	STR_DISP("Close Video")
	STR_HELP("Close the currently open video file")

	void operator()(agi::Context *c) override {
		c->project->CloseVideo();
	}
};

struct video_copy_coordinates final : public validator_video_loaded {
	CMD_NAME("video/copy_coordinates")
	STR_MENU("Copy coordinates to Clipboard")
	STR_DISP("Copy coordinates to Clipboard")
	STR_HELP("Copy the current coordinates of the mouse over the video to the clipboard")

	void operator()(agi::Context *c) override {
		SetClipboard(c->videoDisplay->GetMousePosition().Str());
	}
};

struct video_cycle_subtitles_provider final : public cmd::Command {
	CMD_NAME("video/subtitles_provider/cycle")
	STR_MENU("Cycle active subtitles provider")
	STR_DISP("Cycle active subtitles provider")
	STR_HELP("Cycle through the available subtitles providers")

	void operator()(agi::Context *c) override {
		auto providers = SubtitlesProviderFactory::GetClasses();
		if (providers.empty()) return;

		auto it = find(begin(providers), end(providers), OPT_GET("Subtitle/Provider")->GetString());
		if (it != end(providers)) ++it;
		if (it == end(providers)) it = begin(providers);

		OPT_SET("Subtitle/Provider")->SetString(*it);
		c->frame->StatusTimeout(fmt_tl("Subtitles provider set to %s", *it), 5000);
	}
};

struct video_detach final : public validator_video_loaded {
	CMD_NAME("video/detach")
	CMD_ICON(detach_video_menu)
	STR_MENU("&Detach Video")
	STR_DISP("Detach Video")
	STR_HELP("Detach the video display from the main window, displaying it in a separate Window")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_TOGGLE)

	bool IsActive(const agi::Context *c) override {
		return !!c->dialog->Get<DialogDetachedVideo>();
	}

	void operator()(agi::Context *c) override {
		if (DialogDetachedVideo *d = c->dialog->Get<DialogDetachedVideo>())
			d->Close();
		else
			c->dialog->Show<DialogDetachedVideo>(c);
	}
};

struct video_details final : public validator_video_loaded {
	CMD_NAME("video/details")
	CMD_ICON(show_video_details_menu)
	STR_MENU("Show &Video Details")
	STR_DISP("Show Video Details")
	STR_HELP("Show video details")

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		ShowVideoDetailsDialog(c);
	}
};

struct video_focus_seek final : public validator_video_loaded {
	CMD_NAME("video/focus_seek")
	STR_MENU("Toggle video slider focus")
	STR_DISP("Toggle video slider focus")
	STR_HELP("Toggle focus between the video slider and the previous thing to have focus")

	void operator()(agi::Context *c) override {
		wxWindow *curFocus = wxWindow::FindFocus();
		if (curFocus == c->videoSlider) {
			if (c->previousFocus) c->previousFocus->SetFocus();
		}
		else {
			c->previousFocus = curFocus;
			c->videoSlider->SetFocus();
		}
	}
};

wxImage get_image(agi::Context *c, bool raw) {
	auto frame = c->videoController->GetFrameN();
	return GetImage(*c->project->VideoProvider()->GetFrame(frame, c->project->Timecodes().TimeAtFrame(frame), raw));
}

struct video_frame_copy final : public validator_video_loaded {
	CMD_NAME("video/frame/copy")
	STR_MENU("Copy image to Clipboard")
	STR_DISP("Copy image to Clipboard")
	STR_HELP("Copy the currently displayed frame to the clipboard")

	void operator()(agi::Context *c) override {
		SetClipboard(wxBitmap(get_image(c, false), 24));
	}
};

struct video_frame_copy_raw final : public validator_video_loaded {
	CMD_NAME("video/frame/copy/raw")
	STR_MENU("Copy image to Clipboard (no subtitles)")
	STR_DISP("Copy image to Clipboard (no subtitles)")
	STR_HELP("Copy the currently displayed frame to the clipboard, without the subtitles")

	void operator()(agi::Context *c) override {
		SetClipboard(wxBitmap(get_image(c, true), 24));
	}
};

struct video_frame_next final : public validator_video_loaded {
	CMD_NAME("video/frame/next")
	STR_MENU("Next Frame")
	STR_DISP("Next Frame")
	STR_HELP("Seek to the next frame")

	void operator()(agi::Context *c) override {
		c->videoController->NextFrame();
	}
};

struct video_frame_next_boundary final : public validator_video_loaded {
	CMD_NAME("video/frame/next/boundary")
	STR_MENU("Next Boundary")
	STR_DISP("Next Boundary")
	STR_HELP("Seek to the next beginning or end of a subtitle")

	void operator()(agi::Context *c) override {
		AssDialogue *active_line = c->selectionController->GetActiveLine();
		if (!active_line) return;

		int target = c->videoController->FrameAtTime(active_line->Start, agi::vfr::START);
		if (target > c->videoController->GetFrameN()) {
			c->videoController->JumpToFrame(target);
			return;
		}

		target = c->videoController->FrameAtTime(active_line->End, agi::vfr::END);
		if (target > c->videoController->GetFrameN()) {
			c->videoController->JumpToFrame(target);
			return;
		}

		c->selectionController->NextLine();
		AssDialogue *new_line = c->selectionController->GetActiveLine();
		if (new_line != active_line)
		c->videoController->JumpToTime(new_line->Start);
	}
};

struct video_frame_next_keyframe final : public validator_video_loaded {
	CMD_NAME("video/frame/next/keyframe")
	STR_MENU("Next Keyframe")
	STR_DISP("Next Keyframe")
	STR_HELP("Seek to the next keyframe")

	void operator()(agi::Context *c) override {
		auto const& kf = c->project->Keyframes();
		auto pos = lower_bound(kf.begin(), kf.end(), c->videoController->GetFrameN() + 1);

		c->videoController->JumpToFrame(pos == kf.end() ? c->project->VideoProvider()->GetFrameCount() - 1 : *pos);
	}
};

struct video_frame_next_large final : public validator_video_loaded {
	CMD_NAME("video/frame/next/large")
	STR_MENU("Fast jump forward")
	STR_DISP("Fast jump forward")
	STR_HELP("Fast jump forward")

	void operator()(agi::Context *c) override {
		c->videoController->JumpToFrame(
			c->videoController->GetFrameN() +
			OPT_GET("Video/Slider/Fast Jump Step")->GetInt());
	}
};

struct video_frame_prev final : public validator_video_loaded {
	CMD_NAME("video/frame/prev")
	STR_MENU("Previous Frame")
	STR_DISP("Previous Frame")
	STR_HELP("Seek to the previous frame")

	void operator()(agi::Context *c) override {
		c->videoController->PrevFrame();
	}
};

struct video_frame_prev_boundary final : public validator_video_loaded {
	CMD_NAME("video/frame/prev/boundary")
	STR_MENU("Previous Boundary")
	STR_DISP("Previous Boundary")
	STR_HELP("Seek to the previous beginning or end of a subtitle")

	void operator()(agi::Context *c) override {
		AssDialogue *active_line = c->selectionController->GetActiveLine();
		if (!active_line) return;

		int target = c->videoController->FrameAtTime(active_line->End, agi::vfr::END);
		if (target < c->videoController->GetFrameN()) {
			c->videoController->JumpToFrame(target);
			return;
		}

		target = c->videoController->FrameAtTime(active_line->Start, agi::vfr::START);
		if (target < c->videoController->GetFrameN()) {
			c->videoController->JumpToFrame(target);
			return;
		}

		c->selectionController->PrevLine();
		AssDialogue *new_line = c->selectionController->GetActiveLine();
		if (new_line != active_line)
			c->videoController->JumpToTime(new_line->End, agi::vfr::END);
	}
};

struct video_frame_prev_keyframe final : public validator_video_loaded {
	CMD_NAME("video/frame/prev/keyframe")
	STR_MENU("Previous Keyframe")
	STR_DISP("Previous Keyframe")
	STR_HELP("Seek to the previous keyframe")

	void operator()(agi::Context *c) override {
		auto const& kf = c->project->Keyframes();
		if (kf.empty()) {
			c->videoController->JumpToFrame(0);
			return;
		}

		auto pos = lower_bound(kf.begin(), kf.end(), c->videoController->GetFrameN());

		if (pos != kf.begin())
			--pos;

		c->videoController->JumpToFrame(*pos);
	}
};

struct video_frame_prev_large final : public validator_video_loaded {
	CMD_NAME("video/frame/prev/large")
	STR_MENU("Fast jump backwards")
	STR_DISP("Fast jump backwards")
	STR_HELP("Fast jump backwards")

	void operator()(agi::Context *c) override {
		c->videoController->JumpToFrame(
			c->videoController->GetFrameN() -
			OPT_GET("Video/Slider/Fast Jump Step")->GetInt());
	}
};

static void save_snapshot(agi::Context *c, bool raw) {
	auto option = OPT_GET("Path/Screenshot")->GetString();
	agi::fs::path basepath;

	auto videoname = c->project->VideoName();
	bool is_dummy = boost::starts_with(videoname.string(), "?dummy");

	// Is it a path specifier and not an actual fixed path?
	if (option[0] == '?') {
		// If dummy video is loaded, we can't save to the video location
		if (boost::starts_with(option, "?video") && is_dummy) {
			// So try the script location instead
			option = "?script";
		}
		// Find out where the ?specifier points to
		basepath = c->path->Decode(option);
		// If where ever that is isn't defined, we can't save there
		if ((basepath == "\\") || (basepath == "/")) {
			// So save to the current user's home dir instead
			basepath = wxGetHomeDir().c_str();
		}
	}
	// Actual fixed (possibly relative) path, decode it
	else
		basepath = c->path->MakeAbsolute(option, "?user/");

	basepath /= is_dummy ? "dummy" : videoname.stem();

	// Get full path
	int session_shot_count = 1;
	std::string path;
	do {
		path = agi::format("%s_%03d_%d.png", basepath.string(), session_shot_count++, c->videoController->GetFrameN());
	} while (agi::fs::FileExists(path));

	get_image(c, raw).SaveFile(to_wx(path), wxBITMAP_TYPE_PNG);
}

struct video_frame_save final : public validator_video_loaded {
	CMD_NAME("video/frame/save")
	STR_MENU("Save PNG snapshot")
	STR_DISP("Save PNG snapshot")
	STR_HELP("Save the currently displayed frame to a PNG file in the video's directory")

	void operator()(agi::Context *c) override {
		save_snapshot(c, false);
	}
};

struct video_frame_save_raw final : public validator_video_loaded {
	CMD_NAME("video/frame/save/raw")
	STR_MENU("Save PNG snapshot (no subtitles)")
	STR_DISP("Save PNG snapshot (no subtitles)")
	STR_HELP("Save the currently displayed frame without the subtitles to a PNG file in the video's directory")

	void operator()(agi::Context *c) override {
		save_snapshot(c, true);
	}
};

struct video_jump final : public validator_video_loaded {
	CMD_NAME("video/jump")
	CMD_ICON(jumpto_button)
	STR_MENU("&Jump to...")
	STR_DISP("Jump to")
	STR_HELP("Jump to frame or time")

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		ShowJumpToDialog(c);
		c->videoSlider->SetFocus();
	}
};

struct video_jump_end final : public validator_video_loaded {
	CMD_NAME("video/jump/end")
	CMD_ICON(video_to_subend)
	STR_MENU("Jump Video to &End")
	STR_DISP("Jump Video to End")
	STR_HELP("Jump the video to the end frame of current subtitle")

	void operator()(agi::Context *c) override {
		if (auto active_line = c->selectionController->GetActiveLine())
			c->videoController->JumpToTime(active_line->End, agi::vfr::END);
	}
};

struct video_jump_start final : public validator_video_loaded {
	CMD_NAME("video/jump/start")
	CMD_ICON(video_to_substart)
	STR_MENU("Jump Video to &Start")
	STR_DISP("Jump Video to Start")
	STR_HELP("Jump the video to the start frame of current subtitle")

	void operator()(agi::Context *c) override {
		if (auto active_line = c->selectionController->GetActiveLine())
			c->videoController->JumpToTime(active_line->Start);
	}
};

struct video_open final : public Command {
	CMD_NAME("video/open")
	CMD_ICON(open_video_menu)
	STR_MENU("&Open Video...")
	STR_DISP("Open Video")
	STR_HELP("Open a video file")

	void operator()(agi::Context *c) override {
		auto str = from_wx(_("Video Formats") + " (*.asf,*.avi,*.avs,*.d2v,*.h264,*.hevc,*.m2ts,*.m4v,*.mkv,*.mov,*.mp4,*.mpeg,*.mpg,*.ogm,*.webm,*.wmv,*.ts,*.y4m,*.yuv)|*.asf;*.avi;*.avs;*.d2v;*.h264;*.hevc;*.m2ts;*.m4v;*.mkv;*.mov;*.mp4;*.mpeg;*.mpg;*.ogm;*.webm;*.wmv;*.ts;*.y4m;*.yuv|"
		         + _("All Files") + " (*.*)|*.*");
		auto filename = OpenFileSelector(_("Open video file"), "Path/Last/Video", "", "", str, c->parent);
		if (!filename.empty())
			c->project->LoadVideo(filename);
	}
};

struct video_open_dummy final : public Command {
	CMD_NAME("video/open/dummy")
	CMD_ICON(use_dummy_video_menu)
	STR_MENU("&Use Dummy Video...")
	STR_DISP("Use Dummy Video")
	STR_HELP("Open a placeholder video clip with solid color")

	void operator()(agi::Context *c) override {
		std::string fn = CreateDummyVideo(c->parent);
		if (!fn.empty())
			c->project->LoadVideo(fn);
	}
};

struct video_opt_autoscroll final : public Command {
	CMD_NAME("video/opt/autoscroll")
	CMD_ICON(toggle_video_autoscroll)
	STR_MENU("Toggle autoscroll of video")
	STR_DISP("Toggle autoscroll of video")
	STR_HELP("Toggle automatically seeking video to the start time of selected lines")
	CMD_TYPE(COMMAND_TOGGLE)

	bool IsActive(const agi::Context *) override {
		return OPT_GET("Video/Subtitle Sync")->GetBool();
	}

	void operator()(agi::Context *) override {
		OPT_SET("Video/Subtitle Sync")->SetBool(!OPT_GET("Video/Subtitle Sync")->GetBool());
	}
};

struct video_play final : public validator_video_loaded {
	CMD_NAME("video/play")
	CMD_ICON(button_play)
	STR_MENU("Play")
	STR_DISP("Play")
	STR_HELP("Play video starting on this position")

	void operator()(agi::Context *c) override {
		c->videoController->Play();
	}
};

struct video_play_line final : public validator_video_loaded {
	CMD_NAME("video/play/line")
	CMD_ICON(button_playline)
	STR_MENU("Play line")
	STR_DISP("Play line")
	STR_HELP("Play current line")

	void operator()(agi::Context *c) override {
		c->videoController->PlayLine();
	}
};

struct parsed_line {
	AssDialogue *line;
	std::vector<std::unique_ptr<AssDialogueBlock>> blocks;

	parsed_line(AssDialogue *line) : line(line), blocks(line->ParseTags()) { }
	parsed_line(parsed_line&& r) = default;

	const AssOverrideTag *find_tag(int blockn, std::string const& tag_name, std::string const& alt) const {
		for (auto ovr : blocks | sliced(0, blockn + 1) | reversed | agi::of_type<AssDialogueBlockOverride>()) {
			for (auto const& tag : ovr->Tags | reversed) {
				if (tag.Name == tag_name || tag.Name == alt)
					return &tag;
			}
		}
		return nullptr;
	}

	template<typename T>
	T get_value(int blockn, T initial, std::string const& tag_name, std::string const& alt = "") const {
		auto tag = find_tag(blockn, tag_name, alt);
		if (tag)
			return tag->Params[0].template Get<T>(initial);
		return initial;
	}

	int block_at_pos(int pos) const {
		auto const& text = line->Text.get();
		int n = 0;
		int max = text.size() - 1;
		bool in_block = false;

		for (int i = 0; i <= max; ++i) {
			if (text[i] == '{') {
				if (!in_block && i > 0 && pos >= 0)
					++n;
				in_block = true;
			}
			else if (text[i] == '}' && in_block) {
				in_block = false;
				if (pos > 0 && (i + 1 == max || text[i + 1] != '{'))
					n++;
			}
			else if (!in_block) {
				if (--pos == 0)
					return n + (i < max && text[i + 1] == '{');
			}
		}

		return n - in_block;
	}

	int set_tag(std::string const& tag, std::string const& value, int norm_pos, int orig_pos) {
		int blockn = block_at_pos(norm_pos);

		AssDialogueBlockPlain *plain = nullptr;
		AssDialogueBlockOverride *ovr = nullptr;
		while (blockn >= 0 && !plain && !ovr) {
			AssDialogueBlock *block = blocks[blockn].get();
			switch (block->GetType()) {
			case AssBlockType::PLAIN:
				plain = static_cast<AssDialogueBlockPlain *>(block);
				break;
			case AssBlockType::DRAWING:
				--blockn;
				break;
			case AssBlockType::COMMENT:
				--blockn;
				orig_pos = line->Text.get().rfind('{', orig_pos);
				break;
			case AssBlockType::OVERRIDE:
				ovr = static_cast<AssDialogueBlockOverride*>(block);
				break;
			}
		}

		// If we didn't hit a suitable block for inserting the override just put
		// it at the beginning of the line
		if (blockn < 0)
			orig_pos = 0;

		std::string insert(tag + value);
		int shift = insert.size();
		if (plain || blockn < 0) {
			line->Text = line->Text.get().substr(0, orig_pos) + "{" + insert + "}" + line->Text.get().substr(orig_pos);
			shift += 2;
			blocks = line->ParseTags();
		}
		else if (ovr) {
			std::string alt;
			if (tag == "\\c") alt = "\\1c";
			// Remove old of same
			bool found = false;
			for (size_t i = 0; i < ovr->Tags.size(); i++) {
				std::string const& name = ovr->Tags[i].Name;
				if (tag == name || alt == name) {
					shift -= ((std::string)ovr->Tags[i]).size();
					if (found) {
						ovr->Tags.erase(ovr->Tags.begin() + i);
						i--;
					}
					else {
						ovr->Tags[i].Params[0].Set(value);
						found = true;
					}
				}
			}
			if (!found)
				ovr->AddTag(insert);

			line->UpdateText(blocks);
		}
		else
			assert(false);

		return shift;
	}
};

int normalize_pos(std::string const& text, int pos) {
	int plain_len = 0;
	bool in_block = false;

	for (int i = 0, max = text.size() - 1; i < pos && i <= max; ++i) {
		if (text[i] == '{')
			in_block = true;
		if (!in_block)
			++plain_len;
		if (text[i] == '}' && in_block)
			in_block = false;
	}

	return plain_len;
}

template<typename Func>
void update_lines(const agi::Context *c, wxString const& undo_msg, Func&& f) {
	const auto active_line = c->selectionController->GetActiveLine();
	const int sel_start = c->textSelectionController->GetSelectionStart();
	const int sel_end = c->textSelectionController->GetSelectionEnd();
	const int norm_sel_start = normalize_pos(active_line->Text, sel_start);
	const int norm_sel_end = normalize_pos(active_line->Text, sel_end);
	int active_sel_shift = 0;

	for (const auto line : c->selectionController->GetSelectedSet()) {
		int shift = f(line, sel_start, sel_end, norm_sel_start, norm_sel_end);
		if (line == active_line)
			active_sel_shift = shift;
	}

	auto const& sel = c->selectionController->GetSelectedSet();
	c->ass->Commit(undo_msg, AssFile::COMMIT_DIAG_TEXT, -1, sel.size() == 1 ? *sel.begin() : nullptr);
	if (active_sel_shift != 0)
		c->textSelectionController->SetSelection(sel_start + active_sel_shift, sel_end + active_sel_shift);
}

static void video_set_color_from_cursor(agi::Context *c, const char *tag, const char *alt) {
	Vector2D coords = c->videoDisplay->GetMousePosition();
	wxImage img = get_image(c, false);
	agi::Color new_color(
		img.GetRed(coords.X(), coords.Y()),
		img.GetGreen(coords.X(), coords.Y()),
		img.GetBlue(coords.X(), coords.Y())
	);

	agi::Color initial_color;
	const auto active_line = c->selectionController->GetActiveLine();
	const int sel_start = c->textSelectionController->GetSelectionStart();
	const int sel_end = c->textSelectionController->GetSelectionStart();
	const int norm_sel_start = normalize_pos(active_line->Text, sel_start);

	auto const& sel = c->selectionController->GetSelectedSet();
	using line_info = std::pair<agi::Color, parsed_line>;
	std::vector<line_info> lines;
	for (auto line : sel) {
		AssStyle const* const style = c->ass->GetStyle(line->Style);

		parsed_line parsed(line);
		int blockn = parsed.block_at_pos(norm_sel_start);

		agi::Color color = parsed.get_value(blockn, color, tag, alt);

		if (line == active_line)
			initial_color = color;

		lines.emplace_back(color, std::move(parsed));
	}

	int active_shift = 0;
	int commit_id = -1;
	{
		for (auto& line : lines) {
			int shift = line.second.set_tag(tag, new_color.GetAssOverrideFormatted(), norm_sel_start, sel_start);

			if (line.second.line == active_line)
				active_shift = shift;
		}

		commit_id = c->ass->Commit(_("set color"), AssFile::COMMIT_DIAG_TEXT, commit_id, sel.size() == 1 ? *sel.begin() : nullptr);
		if (active_shift)
			c->textSelectionController->SetSelection(sel_start + active_shift, sel_start + active_shift);
	}
}

struct video_set_color_primary final : public validator_video_loaded {
	CMD_NAME("video/frame/set_color_primary")
	STR_MENU("Set primary fill color")
	STR_DISP("Set primary fill color")
	STR_HELP("Sets the primary fill color (\\c) at the text cursor position from the pixel at the mouse cursor position")

	void operator()(agi::Context *c) override {
		video_set_color_from_cursor(c, "\\c", "\\1c");
	}
};

struct video_set_color_secondary final : public validator_video_loaded {
	CMD_NAME("video/frame/set_color_secondary")
	STR_MENU("Set secondary fill color")
	STR_DISP("Set secondary fill color")
	STR_HELP("Sets the secondary fill color (\\2c) at the text cursor position from the pixel at the mouse cursor position")

	void operator()(agi::Context *c) override {
		video_set_color_from_cursor(c, "\\2c", "");
	}
};

struct video_set_color_outline final : public validator_video_loaded {
	CMD_NAME("video/frame/set_color_outline")
	STR_MENU("Set outline color")
	STR_DISP("Set outline color")
	STR_HELP("Sets the outline color (\\3c) at the text cursor position from the pixel at the mouse cursor position")

	void operator()(agi::Context *c) override {
		video_set_color_from_cursor(c, "\\3c", "");
	}
};

struct video_set_color_shadow final : public validator_video_loaded {
	CMD_NAME("video/frame/set_color_shadow")
	STR_MENU("Set shadow color")
	STR_DISP("Set shadow color")
	STR_HELP("Sets the shadow color (\\4c) at the text cursor position from the pixel at the mouse cursor position")

	void operator()(agi::Context *c) override {
		video_set_color_from_cursor(c, "\\4c", "");
	}
};

struct video_show_overscan final : public validator_video_loaded {
	CMD_NAME("video/show_overscan")
	STR_MENU("Show &Overscan Mask")
	STR_DISP("Show Overscan Mask")
	STR_HELP("Show a mask over the video, indicating areas that might get cropped off by overscan on televisions")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_TOGGLE)

	bool IsActive(const agi::Context *) override {
		return OPT_GET("Video/Overscan Mask")->GetBool();
	}

	void operator()(agi::Context *c) override {
		OPT_SET("Video/Overscan Mask")->SetBool(!OPT_GET("Video/Overscan Mask")->GetBool());
		c->videoDisplay->Render();
	}
};

class video_zoom_100: public validator_video_attached {
public:
	CMD_NAME("video/zoom/100")
	STR_MENU("&100%")
	STR_DISP("100%")
	STR_HELP("Set zoom to 100%")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	bool IsActive(const agi::Context *c) override {
		return c->videoDisplay->GetZoom() == 1.;
	}

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		c->videoDisplay->SetZoom(1.);
	}
};

class video_stop: public validator_video_loaded {
public:
	CMD_NAME("video/stop")
	CMD_ICON(button_pause)
	STR_MENU("Stop video")
	STR_DISP("Stop video")
	STR_HELP("Stop video playback")

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
	}
};

class video_zoom_200: public validator_video_attached {
public:
	CMD_NAME("video/zoom/200")
	STR_MENU("&200%")
	STR_DISP("200%")
	STR_HELP("Set zoom to 200%")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	bool IsActive(const agi::Context *c) override {
		return c->videoDisplay->GetZoom() == 2.;
	}

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		c->videoDisplay->SetZoom(2.);
	}
};

class video_zoom_50: public validator_video_attached {
public:
	CMD_NAME("video/zoom/50")
	STR_MENU("&50%")
	STR_DISP("50%")
	STR_HELP("Set zoom to 50%")
	CMD_TYPE(COMMAND_VALIDATE | COMMAND_RADIO)

	bool IsActive(const agi::Context *c) override {
		return c->videoDisplay->GetZoom() == .5;
	}

	void operator()(agi::Context *c) override {
		c->videoController->Stop();
		c->videoDisplay->SetZoom(.5);
	}
};

struct video_zoom_in final : public validator_video_attached {
	CMD_NAME("video/zoom/in")
	CMD_ICON(zoom_in_button)
	STR_MENU("Zoom In")
	STR_DISP("Zoom In")
	STR_HELP("Zoom video in")

	void operator()(agi::Context *c) override {
		c->videoDisplay->SetZoom(c->videoDisplay->GetZoom() + .125);
	}
};

struct video_zoom_out final : public validator_video_attached {
	CMD_NAME("video/zoom/out")
	CMD_ICON(zoom_out_button)
	STR_MENU("Zoom Out")
	STR_DISP("Zoom Out")
	STR_HELP("Zoom video out")

	void operator()(agi::Context *c) override {
		c->videoDisplay->SetZoom(c->videoDisplay->GetZoom() - .125);
	}
};
}

namespace cmd {
	void init_video() {
		reg(agi::make_unique<video_aspect_cinematic>());
		reg(agi::make_unique<video_aspect_custom>());
		reg(agi::make_unique<video_aspect_default>());
		reg(agi::make_unique<video_aspect_full>());
		reg(agi::make_unique<video_aspect_wide>());
		reg(agi::make_unique<video_close>());
		reg(agi::make_unique<video_copy_coordinates>());
		reg(agi::make_unique<video_cycle_subtitles_provider>());
		reg(agi::make_unique<video_detach>());
		reg(agi::make_unique<video_details>());
		reg(agi::make_unique<video_focus_seek>());
		reg(agi::make_unique<video_frame_copy>());
		reg(agi::make_unique<video_frame_copy_raw>());
		reg(agi::make_unique<video_frame_next>());
		reg(agi::make_unique<video_frame_next_boundary>());
		reg(agi::make_unique<video_frame_next_keyframe>());
		reg(agi::make_unique<video_frame_next_large>());
		reg(agi::make_unique<video_frame_prev>());
		reg(agi::make_unique<video_frame_prev_boundary>());
		reg(agi::make_unique<video_frame_prev_keyframe>());
		reg(agi::make_unique<video_frame_prev_large>());
		reg(agi::make_unique<video_frame_save>());
		reg(agi::make_unique<video_frame_save_raw>());
		reg(agi::make_unique<video_jump>());
		reg(agi::make_unique<video_jump_end>());
		reg(agi::make_unique<video_jump_start>());
		reg(agi::make_unique<video_open>());
		reg(agi::make_unique<video_open_dummy>());
		reg(agi::make_unique<video_opt_autoscroll>());
		reg(agi::make_unique<video_play>());
		reg(agi::make_unique<video_play_line>());
		reg(agi::make_unique<video_set_color_primary>());
		reg(agi::make_unique<video_set_color_secondary>());
		reg(agi::make_unique<video_set_color_outline>());
		reg(agi::make_unique<video_set_color_shadow>());
		reg(agi::make_unique<video_show_overscan>());
		reg(agi::make_unique<video_stop>());
		reg(agi::make_unique<video_zoom_100>());
		reg(agi::make_unique<video_zoom_200>());
		reg(agi::make_unique<video_zoom_50>());
		reg(agi::make_unique<video_zoom_in>());
		reg(agi::make_unique<video_zoom_out>());
	}
}
