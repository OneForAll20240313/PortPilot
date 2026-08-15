// PluginManager 实现
#include "plugin_manager.h"

namespace portpilot::plugin {

// ===== 协议模板扩展点管理 =====

void PluginManager::registerTemplateExtension(
    const std::string& name, std::shared_ptr<IProtocolTemplateExtension> ext) {
    if (!ext) return;
    templateExts_[name] = std::move(ext);
}

void PluginManager::unregisterTemplateExtension(const std::string& name) {
    templateExts_.erase(name);
}

std::vector<std::shared_ptr<IProtocolTemplateExtension>>
PluginManager::templateExtensions() const {
    std::vector<std::shared_ptr<IProtocolTemplateExtension>> result;
    result.reserve(templateExts_.size());
    for (const auto& [_, ext] : templateExts_) {
        result.push_back(ext);
    }
    return result;
}

std::shared_ptr<IProtocolTemplateExtension>
PluginManager::templateExtension(const std::string& name) const {
    auto it = templateExts_.find(name);
    return it != templateExts_.end() ? it->second : nullptr;
}

std::vector<ProtocolTemplate> PluginManager::allTemplates() const {
    std::vector<ProtocolTemplate> result;
    for (const auto& [_, ext] : templateExts_) {
        if (!ext) continue;
        auto templates = ext->listTemplates();
        result.insert(result.end(), templates.begin(), templates.end());
    }
    return result;
}

// ===== 自定义命令扩展点管理 =====

void PluginManager::registerCommandExtension(
    const std::string& name, std::shared_ptr<ICommandExtension> ext) {
    if (!ext) return;
    commandExts_[name] = std::move(ext);
}

void PluginManager::unregisterCommandExtension(const std::string& name) {
    commandExts_.erase(name);
}

std::vector<std::shared_ptr<ICommandExtension>>
PluginManager::commandExtensions() const {
    std::vector<std::shared_ptr<ICommandExtension>> result;
    result.reserve(commandExts_.size());
    for (const auto& [_, ext] : commandExts_) {
        result.push_back(ext);
    }
    return result;
}

std::shared_ptr<ICommandExtension>
PluginManager::commandExtension(const std::string& name) const {
    auto it = commandExts_.find(name);
    return it != commandExts_.end() ? it->second : nullptr;
}

std::vector<ScriptData> PluginManager::allScripts() const {
    std::vector<ScriptData> result;
    for (const auto& [_, ext] : commandExts_) {
        if (!ext) continue;
        auto scripts = ext->listScripts();
        result.insert(result.end(), scripts.begin(), scripts.end());
    }
    return result;
}

// ===== 计数 =====

size_t PluginManager::templateExtensionCount() const {
    return templateExts_.size();
}

size_t PluginManager::commandExtensionCount() const {
    return commandExts_.size();
}

} // namespace portpilot::plugin
