#include "KeyboardShortcuts.h"
#include "Logger.h"
#include <fstream>
#include <sstream>

#include <SDL2/SDL_keyboard.h>

namespace ed
{
	KeyboardShortcuts::KeyboardShortcuts()
	{
		m_keys[0] = m_keys[1] = -1;
	}
	void KeyboardShortcuts::Load()
	{
		ed::Logger::Get().Log("Loading shortcut information");

		std::ifstream file("data/shortcuts.kb");
		std::string str;

		while (std::getline(file, str)) {
			std::stringstream ss(str);
			std::string name, token;

			ss >> name;

			if (name.empty()) continue;

			m_data[name].Key1 = -1;
			m_data[name].Key2 = -1;
			while (ss >> token)
			{
				if (token == "CTRL") m_data[name].Ctrl = true;
				else if (token == "ALT") m_data[name].Alt = true;
				else if (token == "SHIFT") m_data[name].Shift = true;
				else if (token == "NONE") break;
				else {
					if (m_data[name].Key1 == -1)
						m_data[name].Key1 = SDL_GetKeyFromName(token.c_str());
					else if (m_data[name].Key2 == -1)
						m_data[name].Key2 = SDL_GetKeyFromName(token.c_str());
				}				
			}
		}

		ed::Logger::Get().Log("Loaded shortcut information");
	}
	void KeyboardShortcuts::Save()
	{
		std::ofstream file("data/shortcuts.kb");
		std::string str;
		
		for (auto& s : m_data) {
			if (s.second.Key1 == -1) {
				//file << " NONE" << std::endl;
				continue;
			}

			file << s.first;

			if (s.second.Ctrl)
				file << " CTRL";
			if (s.second.Alt)
				file << " ALT";
			if (s.second.Shift)
				file << " SHIFT";
			file << " " << SDL_GetKeyName(s.second.Key1);
			if (s.second.Key2 != -1)
				file << " " << SDL_GetKeyName(s.second.Key2);

			file << std::endl;
		}

		ed::Logger::Get().Log("Saved shortcut information");

		return;
	}
	bool KeyboardShortcuts::Set(const std::string& name, int VK1, int VK2, bool alt, bool ctrl, bool shift)
	{
		if (VK1 == -1 || (alt == false && ctrl == false && shift == false && !m_canSolo(name, VK1)))
			return false;

		// TODO: maybe this log is too much?
		Logger::Get().Log("Set keyboard shortcut (" + name + ")");

		for (auto& i : m_data)
			if (i.second.Ctrl == ctrl && i.second.Alt == alt && i.second.Shift == shift && i.second.Key1 == VK1 && (VK2 == -1 || i.second.Key2 == VK2 || i.second.Key2 == -1)) {
				if (!(name == "CodeUI.Save" && i.first == "Project.Save") &&
					!(name == "Project.Save" && i.first == "CodeUI.Save"))
				{
					i.second.Ctrl = i.second.Alt = i.second.Shift = false;
					i.second.Key1 = i.second.Key2 = -1;
				}
			}
		m_data[name].Alt = alt;
		m_data[name].Ctrl = ctrl;
		m_data[name].Shift = shift;
		m_data[name].Key1 = VK1;
		m_data[name].Key2 = VK2;

		return true;
	}
	void KeyboardShortcuts::SetCallback(const std::string& name, std::function<void()> func)
	{
		m_data[name].Function = func;
	}
	std::string KeyboardShortcuts::GetString(const std::string& name)
	{
		if (m_data[name].Key1 == -1 || (m_data[name].Key1 == 0 && m_data[name].Key2 == 0))
			return "NONE";

		std::string ret = "";

		if (m_data[name].Ctrl)
			ret += "CTRL+";
		if (m_data[name].Alt)
			ret += "ALT+";
		if (m_data[name].Shift)
			ret += "SHIFT+";
		ret += std::string(SDL_GetKeyName(m_data[name].Key1)) + "+";
		if (m_data[name].Key2 != -1)
			ret += std::string(SDL_GetKeyName(m_data[name].Key2)) + "+";

		return ret.substr(0, ret.size() - 1);
	}
	std::vector<std::string> KeyboardShortcuts::GetNameList()
	{
		std::vector<std::string> ret;
		for (auto i : m_data)
			ret.push_back(i.first);
		return ret;
	}
	void KeyboardShortcuts::Check(const SDL_Event& e, bool codeHasFocus)
	{
		// dont process key repeats
		if (e.key.repeat != 0)
			return;

		m_keys[0] = m_keys[1];
		m_keys[1] = e.key.keysym.sym;

		bool alt = e.key.keysym.mod & KMOD_ALT;
		bool ctrl = e.key.keysym.mod & KMOD_CTRL;
		bool shift = e.key.keysym.mod & KMOD_SHIFT;

		bool resetSecond = false, resetFirst = false;

		for (auto hotkey : m_data) {
			if (codeHasFocus && !(hotkey.first.find("Editor") != std::string::npos || hotkey.first.find("CodeUI") != std::string::npos || hotkey.first == "Project.Save"))
				continue;

			Shortcut s = hotkey.second;
			if (s.Alt == alt && s.Ctrl == ctrl && s.Shift == shift) {
				int key2 = m_keys[1];
				if (s.Key2 == -1 && s.Key1 == key2 && s.Function != nullptr) {

					/*
					 [ 'G', 'S' ] -> it would call the CTRL+S instead of CTRL+G+S shortcut.
					 That is why we check if we actually meant CTRL+S or CTRL+G+S
					*/
					bool found = false;
					int key1 = m_keys[0];
					if (key1 != -1)
						for (auto clone : m_data)
							if (clone.second.Alt == alt && clone.second.Ctrl == ctrl && clone.second.Shift == shift &&
								clone.second.Key1 == key1 && clone.second.Key2 == key2 && clone.second.Key2 != -1)
							{
								found = true;
							}

					// call the proper function
					if (!found) {
						s.Function();
						resetSecond = true;
					}
				}
				else if (s.Key2 != -1) {
					if (m_keys[0] != -1) {
						int key1 = m_keys[0];

						if (s.Key1 == key1 && s.Key2 == key2 && s.Function != nullptr) {
							s.Function();

							resetFirst = resetSecond = true;
						}
					}
				}
			}
		}

		if (resetFirst) m_keys[0] = -1;
		if (resetSecond) m_keys[1] = -1;
	}
	bool KeyboardShortcuts::m_canSolo(const std::string& name, int k)
	{
		return (k >= SDLK_F1 && k <= SDLK_F12) || (k >= SDLK_F13 && k <= SDLK_F24) || (name.find("Editor") == std::string::npos);
	}
}