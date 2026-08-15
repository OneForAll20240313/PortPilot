// Plugin 扩展层单元测试
// 覆盖：PluginManager 注册/注销/查询、扩展点接口操作、数据结构完整性、嵌套结构
// 对齐契约：service-api.md A-413 manageTemplates / A-715 manageScripts
#include <gtest/gtest.h>

#include "plugin/plugin_manager.h"

#include <algorithm>

using namespace portpilot::plugin;

// ===== 测试用内存实现 =====

/// 内存协议模板扩展点（测试用，实现 IProtocolTemplateExtension）
class InMemoryTemplateExtension : public IProtocolTemplateExtension {
public:
    void manageTemplate(TemplateAction action, const ProtocolTemplate& tmpl) override {
        switch (action) {
        case TemplateAction::Add:
            templates_.push_back(tmpl);
            break;
        case TemplateAction::Update: {
            auto it = std::find_if(templates_.begin(), templates_.end(),
                [&](const ProtocolTemplate& t) { return t.id == tmpl.id; });
            if (it != templates_.end()) *it = tmpl;
            break;
        }
        case TemplateAction::Delete:
            templates_.erase(
                std::remove_if(templates_.begin(), templates_.end(),
                    [&](const ProtocolTemplate& t) { return t.id == tmpl.id; }),
                templates_.end());
            break;
        case TemplateAction::List:
            break;
        }
    }
    std::vector<ProtocolTemplate> listTemplates() const override { return templates_; }

private:
    std::vector<ProtocolTemplate> templates_;
};

/// 内存命令扩展点（测试用，实现 ICommandExtension）
class InMemoryCommandExtension : public ICommandExtension {
public:
    void manageScript(ScriptAction action, const ScriptData& script) override {
        switch (action) {
        case ScriptAction::Import:
            scripts_.push_back(script);
            break;
        case ScriptAction::Edit: {
            auto it = std::find_if(scripts_.begin(), scripts_.end(),
                [&](const ScriptData& s) { return s.id == script.id; });
            if (it != scripts_.end()) *it = script;
            break;
        }
        case ScriptAction::Delete:
            scripts_.erase(
                std::remove_if(scripts_.begin(), scripts_.end(),
                    [&](const ScriptData& s) { return s.id == script.id; }),
                scripts_.end());
            break;
        case ScriptAction::List:
            break;
        }
    }
    std::vector<ScriptData> listScripts() const override { return scripts_; }

private:
    std::vector<ScriptData> scripts_;
};

// ===== 辅助工厂函数 =====

ProtocolTemplate makeTemplate(const std::string& id, const std::string& name) {
    ProtocolTemplate t;
    t.id = id;
    t.name = name;
    t.schemaType = SchemaType::Custom;
    t.lengthType = LengthType::Fixed;
    t.frameDef.fixedLength = 8;
    t.frameDef.startPattern = "7E";
    ProtocolField f;
    f.name = "cmd";
    f.type = FieldType::UInt8;
    f.offset = 0;
    t.fields.push_back(f);
    return t;
}

ScriptData makeScript(const std::string& id, const std::string& name) {
    ScriptData s;
    s.id = id;
    s.sessionId = "session-1";
    s.name = name;
    s.createdAt = 1000;
    CommandItem cmd;
    cmd.id = "cmd-1";
    cmd.type = CommandType::Send;
    cmd.data = "AABBCC";
    s.commands.push_back(cmd);
    return s;
}

// ===== PluginManager 注册/注销/查询 =====

TEST(PluginManagerTest, RegisterAndQueryTemplateExtension) {
    PluginManager mgr;
    auto ext = std::make_shared<InMemoryTemplateExtension>();
    mgr.registerTemplateExtension("default", ext);
    EXPECT_EQ(mgr.templateExtensionCount(), 1u);
    EXPECT_EQ(mgr.templateExtension("default"), ext);
}

TEST(PluginManagerTest, UnregisterTemplateExtension) {
    PluginManager mgr;
    auto ext = std::make_shared<InMemoryTemplateExtension>();
    mgr.registerTemplateExtension("default", ext);
    EXPECT_EQ(mgr.templateExtensionCount(), 1u);
    mgr.unregisterTemplateExtension("default");
    EXPECT_EQ(mgr.templateExtensionCount(), 0u);
    EXPECT_EQ(mgr.templateExtension("default"), nullptr);
}

TEST(PluginManagerTest, RegisterAndQueryCommandExtension) {
    PluginManager mgr;
    auto ext = std::make_shared<InMemoryCommandExtension>();
    mgr.registerCommandExtension("default", ext);
    EXPECT_EQ(mgr.commandExtensionCount(), 1u);
    EXPECT_EQ(mgr.commandExtension("default"), ext);
}

TEST(PluginManagerTest, UnregisterCommandExtension) {
    PluginManager mgr;
    auto ext = std::make_shared<InMemoryCommandExtension>();
    mgr.registerCommandExtension("default", ext);
    mgr.unregisterCommandExtension("default");
    EXPECT_EQ(mgr.commandExtensionCount(), 0u);
}

// ===== 空管理器边界条件 =====

TEST(PluginManagerTest, EmptyManagerReturnsEmpty) {
    PluginManager mgr;
    EXPECT_TRUE(mgr.templateExtensions().empty());
    EXPECT_TRUE(mgr.commandExtensions().empty());
    EXPECT_TRUE(mgr.allTemplates().empty());
    EXPECT_TRUE(mgr.allScripts().empty());
    EXPECT_EQ(mgr.templateExtension("nonexistent"), nullptr);
    EXPECT_EQ(mgr.commandExtension("nonexistent"), nullptr);
    EXPECT_EQ(mgr.templateExtensionCount(), 0u);
    EXPECT_EQ(mgr.commandExtensionCount(), 0u);
}

// ===== 重复注册覆盖 =====

TEST(PluginManagerTest, DuplicateRegistrationOverwrites) {
    PluginManager mgr;
    auto ext1 = std::make_shared<InMemoryTemplateExtension>();
    auto ext2 = std::make_shared<InMemoryTemplateExtension>();
    mgr.registerTemplateExtension("default", ext1);
    mgr.registerTemplateExtension("default", ext2);
    EXPECT_EQ(mgr.templateExtensionCount(), 1u);
    EXPECT_EQ(mgr.templateExtension("default"), ext2);
}

// ===== 注销不存在的扩展点（无副作用）=====

TEST(PluginManagerTest, UnregisterNonexistentIsNoop) {
    PluginManager mgr;
    mgr.unregisterTemplateExtension("nonexistent");
    mgr.unregisterCommandExtension("nonexistent");
    EXPECT_EQ(mgr.templateExtensionCount(), 0u);
    EXPECT_EQ(mgr.commandExtensionCount(), 0u);
}

// ===== nullptr 注册被拒绝 =====

TEST(PluginManagerTest, RegisterNullptrTemplateExtensionIsIgnored) {
    PluginManager mgr;
    mgr.registerTemplateExtension("null", nullptr);
    EXPECT_EQ(mgr.templateExtensionCount(), 0u);
    EXPECT_EQ(mgr.templateExtension("null"), nullptr);
}

TEST(PluginManagerTest, RegisterNullptrCommandExtensionIsIgnored) {
    PluginManager mgr;
    mgr.registerCommandExtension("null", nullptr);
    EXPECT_EQ(mgr.commandExtensionCount(), 0u);
    EXPECT_EQ(mgr.commandExtension("null"), nullptr);
}

// ===== 多扩展点注册与枚举 =====

TEST(PluginManagerTest, MultipleExtensionsEnumerated) {
    PluginManager mgr;
    mgr.registerTemplateExtension("ext1", std::make_shared<InMemoryTemplateExtension>());
    mgr.registerTemplateExtension("ext2", std::make_shared<InMemoryTemplateExtension>());
    mgr.registerCommandExtension("cmd1", std::make_shared<InMemoryCommandExtension>());
    mgr.registerCommandExtension("cmd2", std::make_shared<InMemoryCommandExtension>());
    EXPECT_EQ(mgr.templateExtensionCount(), 2u);
    EXPECT_EQ(mgr.commandExtensionCount(), 2u);
    EXPECT_EQ(mgr.templateExtensions().size(), 2u);
    EXPECT_EQ(mgr.commandExtensions().size(), 2u);
}

// ===== 聚合查询 =====

TEST(PluginManagerTest, AggregateAllTemplates) {
    PluginManager mgr;
    auto ext1 = std::make_shared<InMemoryTemplateExtension>();
    auto ext2 = std::make_shared<InMemoryTemplateExtension>();
    ext1->manageTemplate(TemplateAction::Add, makeTemplate("t1", "Modbus"));
    ext1->manageTemplate(TemplateAction::Add, makeTemplate("t2", "LIN"));
    ext2->manageTemplate(TemplateAction::Add, makeTemplate("t3", "CAN"));
    mgr.registerTemplateExtension("ext1", ext1);
    mgr.registerTemplateExtension("ext2", ext2);
    auto all = mgr.allTemplates();
    EXPECT_EQ(all.size(), 3u);
}

TEST(PluginManagerTest, AggregateAllScripts) {
    PluginManager mgr;
    auto ext1 = std::make_shared<InMemoryCommandExtension>();
    auto ext2 = std::make_shared<InMemoryCommandExtension>();
    ext1->manageScript(ScriptAction::Import, makeScript("s1", "Init"));
    ext2->manageScript(ScriptAction::Import, makeScript("s2", "Teardown"));
    mgr.registerCommandExtension("ext1", ext1);
    mgr.registerCommandExtension("ext2", ext2);
    auto all = mgr.allScripts();
    EXPECT_EQ(all.size(), 2u);
}

// ===== 扩展点接口操作（A-413 manageTemplates）=====

TEST(ProtocolTemplateExtensionTest, AddUpdateDelete) {
    InMemoryTemplateExtension ext;
    EXPECT_TRUE(ext.listTemplates().empty());

    // Add
    auto t = makeTemplate("p1", "Modbus RTU");
    ext.manageTemplate(TemplateAction::Add, t);
    EXPECT_EQ(ext.listTemplates().size(), 1u);
    EXPECT_EQ(ext.listTemplates()[0].name, "Modbus RTU");

    // Update
    t.name = "Modbus TCP";
    ext.manageTemplate(TemplateAction::Update, t);
    EXPECT_EQ(ext.listTemplates().size(), 1u);
    EXPECT_EQ(ext.listTemplates()[0].name, "Modbus TCP");

    // Delete
    ext.manageTemplate(TemplateAction::Delete, t);
    EXPECT_TRUE(ext.listTemplates().empty());
}

TEST(ProtocolTemplateExtensionTest, UpdateNonexistentIsNoop) {
    InMemoryTemplateExtension ext;
    auto t = makeTemplate("p1", "Modbus");
    ext.manageTemplate(TemplateAction::Update, t); // 不存在，无副作用
    EXPECT_TRUE(ext.listTemplates().empty());
}

TEST(ProtocolTemplateExtensionTest, DeleteNonexistentIsNoop) {
    InMemoryTemplateExtension ext;
    auto t = makeTemplate("p1", "Modbus");
    ext.manageTemplate(TemplateAction::Delete, t); // 不存在，无副作用
    EXPECT_TRUE(ext.listTemplates().empty());
}

// ===== 扩展点接口操作（A-715 manageScripts）=====

TEST(ScriptExtensionTest, ImportEditDelete) {
    InMemoryCommandExtension ext;
    EXPECT_TRUE(ext.listScripts().empty());

    // Import
    auto s = makeScript("c1", "Boot Sequence");
    ext.manageScript(ScriptAction::Import, s);
    EXPECT_EQ(ext.listScripts().size(), 1u);
    EXPECT_EQ(ext.listScripts()[0].name, "Boot Sequence");

    // Edit
    s.name = "Shutdown Sequence";
    ext.manageScript(ScriptAction::Edit, s);
    EXPECT_EQ(ext.listScripts().size(), 1u);
    EXPECT_EQ(ext.listScripts()[0].name, "Shutdown Sequence");

    // Delete
    ext.manageScript(ScriptAction::Delete, s);
    EXPECT_TRUE(ext.listScripts().empty());
}

TEST(ScriptExtensionTest, EditNonexistentIsNoop) {
    InMemoryCommandExtension ext;
    auto s = makeScript("c1", "Test");
    ext.manageScript(ScriptAction::Edit, s); // 不存在，无副作用
    EXPECT_TRUE(ext.listScripts().empty());
}

TEST(ScriptExtensionTest, DeleteNonexistentIsNoop) {
    InMemoryCommandExtension ext;
    auto s = makeScript("c1", "Test");
    ext.manageScript(ScriptAction::Delete, s); // 不存在，无副作用
    EXPECT_TRUE(ext.listScripts().empty());
}

// ===== 数据结构完整性（对齐 schema）=====

TEST(ProtocolTemplateTest, StructureIntegrity) {
    auto t = makeTemplate("proto-1", "TestProtocol");
    EXPECT_EQ(t.id, "proto-1");
    EXPECT_EQ(t.name, "TestProtocol");
    EXPECT_EQ(t.schemaType, SchemaType::Custom);
    EXPECT_EQ(t.lengthType, LengthType::Fixed);
    EXPECT_EQ(t.frameDef.fixedLength, 8);
    EXPECT_EQ(t.frameDef.startPattern, "7E");
    ASSERT_EQ(t.fields.size(), 1u);
    EXPECT_EQ(t.fields[0].name, "cmd");
    EXPECT_EQ(t.fields[0].type, FieldType::UInt8);
}

TEST(ProtocolTemplateTest, VariableLengthWithLengthField) {
    ProtocolTemplate t;
    t.id = "var-1";
    t.name = "VariableProto";
    t.lengthType = LengthType::Variable;
    t.frameDef.lengthField.offset = 1;
    t.frameDef.lengthField.width = 2;
    t.frameDef.lengthField.byteOrder = ByteOrder::Big;
    EXPECT_EQ(t.frameDef.lengthField.offset, 1);
    EXPECT_EQ(t.frameDef.lengthField.width, 2);
    EXPECT_EQ(t.frameDef.lengthField.byteOrder, ByteOrder::Big);
}

TEST(ScriptDataTest, StructureIntegrity) {
    auto s = makeScript("script-1", "TestScript");
    EXPECT_EQ(s.id, "script-1");
    EXPECT_EQ(s.sessionId, "session-1");
    EXPECT_EQ(s.name, "TestScript");
    EXPECT_EQ(s.createdAt, 1000);
    EXPECT_TRUE(s.enabled);
    EXPECT_EQ(s.state, CommandState::Pending);
    ASSERT_EQ(s.commands.size(), 1u);
    EXPECT_EQ(s.commands[0].id, "cmd-1");
    EXPECT_EQ(s.commands[0].type, CommandType::Send);
    EXPECT_EQ(s.commands[0].data, "AABBCC");
}

// ===== 命令项递归嵌套（对齐 command-group.schema.json children）=====

TEST(CommandItemTest, NestedLoopChildren) {
    CommandItem loop;
    loop.id = "loop-1";
    loop.type = CommandType::Loop;
    loop.loopCount = 3;

    CommandItem send;
    send.id = "send-1";
    send.type = CommandType::Send;
    send.data = "01";
    loop.children.push_back(send);

    ScriptData s;
    s.id = "s1";
    s.sessionId = "sess";
    s.name = "Loop Test";
    s.createdAt = 1;
    s.commands.push_back(loop);

    ASSERT_EQ(s.commands.size(), 1u);
    EXPECT_EQ(s.commands[0].type, CommandType::Loop);
    EXPECT_EQ(s.commands[0].loopCount, 3);
    ASSERT_EQ(s.commands[0].children.size(), 1u);
    EXPECT_EQ(s.commands[0].children[0].type, CommandType::Send);
    EXPECT_EQ(s.commands[0].children[0].data, "01");
}

TEST(CommandItemTest, DeeplyNestedChildren) {
    // loop -> condition -> send
    CommandItem inner;
    inner.id = "send-inner";
    inner.type = CommandType::Send;
    inner.data = "FF";

    CommandItem cond;
    cond.id = "cond-1";
    cond.type = CommandType::Condition;
    cond.children.push_back(inner);

    CommandItem loop;
    loop.id = "loop-1";
    loop.type = CommandType::Loop;
    loop.loopCount = 5;
    loop.children.push_back(cond);

    ScriptData s;
    s.id = "s1";
    s.sessionId = "sess";
    s.name = "Deep Nest";
    s.createdAt = 1;
    s.commands.push_back(loop);

    ASSERT_EQ(s.commands.size(), 1u);
    ASSERT_EQ(s.commands[0].children.size(), 1u);
    ASSERT_EQ(s.commands[0].children[0].children.size(), 1u);
    EXPECT_EQ(s.commands[0].children[0].children[0].data, "FF");
}
