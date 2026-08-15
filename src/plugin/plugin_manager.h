// PluginManager：扩展点注册中心
// 架构定位：Plugin 层核心组件，管理协议模板扩展点和自定义命令扩展点的注册/注销/查询
// Service 层通过 PluginManager 访问已注册的扩展点（见 13.1 / 14.10.2）
#ifndef PORTPILOT_PLUGIN_PLUGIN_MANAGER_H
#define PORTPILOT_PLUGIN_PLUGIN_MANAGER_H

#include "plugin_extension.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace portpilot::plugin {

class PluginManager {
public:
    PluginManager() = default;
    ~PluginManager() = default;

    // 禁止拷贝，允许移动
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;
    PluginManager(PluginManager&&) = default;
    PluginManager& operator=(PluginManager&&) = default;

    // ===== 协议模板扩展点管理 =====

    /// 注册协议模板扩展点
    /// \param name 扩展点唯一标识
    /// \param ext 扩展点实例
    void registerTemplateExtension(const std::string& name,
                                   std::shared_ptr<IProtocolTemplateExtension> ext);

    /// 注销协议模板扩展点
    void unregisterTemplateExtension(const std::string& name);

    /// 查询所有已注册的协议模板扩展点
    std::vector<std::shared_ptr<IProtocolTemplateExtension>> templateExtensions() const;

    /// 按名称查询协议模板扩展点（未找到返回 nullptr）
    std::shared_ptr<IProtocolTemplateExtension> templateExtension(const std::string& name) const;

    /// 聚合所有扩展点的模板列表
    std::vector<ProtocolTemplate> allTemplates() const;

    // ===== 自定义命令扩展点管理 =====

    /// 注册自定义命令扩展点
    void registerCommandExtension(const std::string& name,
                                  std::shared_ptr<ICommandExtension> ext);

    /// 注销自定义命令扩展点
    void unregisterCommandExtension(const std::string& name);

    /// 查询所有已注册的自定义命令扩展点
    std::vector<std::shared_ptr<ICommandExtension>> commandExtensions() const;

    /// 按名称查询自定义命令扩展点（未找到返回 nullptr）
    std::shared_ptr<ICommandExtension> commandExtension(const std::string& name) const;

    /// 聚合所有扩展点的脚本列表
    std::vector<ScriptData> allScripts() const;

    // ===== 计数 =====
    size_t templateExtensionCount() const;
    size_t commandExtensionCount() const;

private:
    std::unordered_map<std::string, std::shared_ptr<IProtocolTemplateExtension>> templateExts_;
    std::unordered_map<std::string, std::shared_ptr<ICommandExtension>> commandExts_;
};

} // namespace portpilot::plugin

#endif // PORTPILOT_PLUGIN_PLUGIN_MANAGER_H
