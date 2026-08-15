// Plugin 扩展点接口定义
// 对齐契约：service-api.md A-413 manageTemplates / A-715 manageScripts
// 架构定位：Plugin 层提供扩展点接口，Service 层通过 PluginManager 挂载并调用
#ifndef PORTPILOT_PLUGIN_PLUGIN_EXTENSION_H
#define PORTPILOT_PLUGIN_PLUGIN_EXTENSION_H

#include "plugin_types.h"

#include <memory>
#include <vector>

namespace portpilot::plugin {

/// 协议模板扩展点接口（对齐 A-413 manageTemplates）
///
/// Plugin 层提供此接口，ProtocolService 通过 PluginManager 挂载并调用。
/// 实现方负责协议模板的增删改查，数据结构遵循 protocol.schema.json。
class IProtocolTemplateExtension {
public:
    virtual ~IProtocolTemplateExtension() = default;

    /// 管理协议模板（add/update/delete）
    /// \param action 操作类型（List 操作请用 listTemplates）
    /// \param tmpl 模板数据
    virtual void manageTemplate(TemplateAction action, const ProtocolTemplate& tmpl) = 0;

    /// 列出所有协议模板
    virtual std::vector<ProtocolTemplate> listTemplates() const = 0;
};

/// 自定义命令扩展点接口（对齐 A-715 manageScripts）
///
/// Plugin 层提供此接口，CommandService 通过 PluginManager 挂载并调用。
/// 实现方负责脚本/命令组的增删改查，数据结构遵循 command-group.schema.json。
class ICommandExtension {
public:
    virtual ~ICommandExtension() = default;

    /// 管理脚本（import/edit/delete）
    /// \param action 操作类型（List 操作请用 listScripts）
    /// \param script 脚本数据
    virtual void manageScript(ScriptAction action, const ScriptData& script) = 0;

    /// 列出所有脚本
    virtual std::vector<ScriptData> listScripts() const = 0;
};

} // namespace portpilot::plugin

#endif // PORTPILOT_PLUGIN_PLUGIN_EXTENSION_H
