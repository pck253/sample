using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;

namespace code_gen
{
    public class ZppBitsPacketCodegen : ICodegen
    {
        const string FileLayout = @"#pragma once

namespace {NamespaceName}
{
    enum class EProtocol : Protocol_t
    {
        Invalid = 0,
{EnumItems}
        Max
    };

{Structs}
}";

        const string ServerStructLayout = @"    struct {StructName} : public ServerPacketBase
    {
{FieldDeclarations}

        {StructName}() : ServerPacketBase(EServerPacketType::{NamespaceName}, static_cast<Protocol_t>(EProtocol::{StructName})) {}
        
{CustomConstructor}

        template <typename Archive, typename Self>
        static zpp::bits::errc serialize(Archive & ar, Self & self)
        {
            if constexpr (Archive::kind() == zpp::bits::kind::in)
            {
                return ar({SerializeFields});
            }
            ADD_BASE_ARCHIVE()
            return ar({SerializeFields});
        }
    };
";
        const string ServerCustomConstructLayout = @"        explicit {StructName}(
{ConstructorParams})
            : ServerPacketBase(EServerPacketType::{NamespaceName}, static_cast<Protocol_t>(EProtocol::{StructName})),
{ConstructorInitializers}
        {}
";

        const string ClientStructLayout = @"    struct {StructName} : public ClientPacketBase
    {
{FieldDeclarations}

        {StructName}() : ClientPacketBase(static_cast<Protocol_t>(EProtocol::{StructName})) {}
        
{CustomConstructor}

        template <typename Archive, typename Self>
        static zpp::bits::errc serialize(Archive & ar, Self & self)
        {
            if constexpr (Archive::kind() == zpp::bits::kind::in)
            {
                return ar({SerializeFields});
            }
            ADD_BASE_ARCHIVE()
            return ar({SerializeFields});
        }
    };
";
        const string ClientCustomConstructLayout = @"        explicit {StructName}(
{ConstructorParams})
            : ClientPacketBase(static_cast<Protocol_t>(EProtocol::{StructName})),
{ConstructorInitializers}
        {}
";

        public void Generate(string inputPath, string outputPath)
        {
            if (!Directory.Exists(inputPath)) return;
            if (!Directory.Exists(outputPath)) Directory.CreateDirectory(outputPath);

            // Look for JSON files instead of TXT
            foreach (var file in Directory.GetFiles(inputPath, "*.json"))
            {
                string rawFileName = Path.GetFileNameWithoutExtension(file);
                string nsName = ToPascalCase(rawFileName);
                string outputFileName = ToSnakeCase(nsName) + ".h";

                var content = File.ReadAllText(file);

                JsonDocument doc;
                try
                {
                    doc = JsonDocument.Parse(content);
                }
                catch
                {
                    // Invalid JSON skip
                    continue;
                }

                var root = doc.RootElement;

                // Determine server/client mode for the whole file. Support multiple input styles.
                bool isServerFile = false;
                if (root.TryGetProperty("server", out var serverProp) && serverProp.ValueKind == JsonValueKind.True)
                {
                    isServerFile = true;
                }
                else if (root.TryGetProperty("mode", out var modeProp) && modeProp.ValueKind == JsonValueKind.String)
                {
                    var mode = modeProp.GetString();
                    if (!string.IsNullOrEmpty(mode) && mode.Equals("server", System.StringComparison.OrdinalIgnoreCase))
                        isServerFile = true;
                }

                var enumList = new List<string>();
                var structList = new List<string>();

                if (!root.TryGetProperty("packets", out var packetsProp) || packetsProp.ValueKind != JsonValueKind.Array)
                {
                    // No packets array, skip
                    continue;
                }

                foreach (var pkt in packetsProp.EnumerateArray())
                {
                    if (pkt.ValueKind != JsonValueKind.Object)
                    {
                        continue;
                    }
                    if (!pkt.TryGetProperty("name", out var nameProp) || nameProp.ValueKind != JsonValueKind.String)
                    {
                        continue;
                    }

                    string structName = nameProp.GetString();
                    if (string.IsNullOrWhiteSpace(structName))
                    {
                        continue;
                    }

                    enumList.Add($"        {structName},");

                    var fields = new List<(string Type, string Name, bool IsCustom)>();

                    if (pkt.TryGetProperty("fields", out var fieldsProp) && fieldsProp.ValueKind == JsonValueKind.Array)
                    {
                        foreach (var f in fieldsProp.EnumerateArray())
                        {
                            if (f.ValueKind != JsonValueKind.Object) continue;
                            if (!f.TryGetProperty("type", out var tprop) || tprop.ValueKind != JsonValueKind.String)
                            {
                                continue;
                            }

                            if (!f.TryGetProperty("name", out var nprop) || nprop.ValueKind != JsonValueKind.String)
                            {
                                continue;
                            }

                            string ftype = tprop.GetString();
                            string fname = nprop.GetString();

                            bool isCustom = false;

                            if (f.TryGetProperty("isCustom", out var ic) && ic.ValueKind == JsonValueKind.True)
                            {
                                isCustom = true;
                            }

                            fields.Add((ftype, fname, isCustom));
                        }
                    }

                    string fieldDecls = string.Join("\n", fields.Select(f => $"        {f.Type} {f.Name}{(f.IsCustom ? ";" : "{};")}"));
                    string constParams = string.Join(",\n", fields.Select(f => $"            const {f.Type}{(f.IsCustom ? "&" : "")} _{f.Name}"));
                    string constInits = string.Join(",\n", fields.Select(f => $"            {f.Name}(_{f.Name})"));
                    string serializeList = string.Join(", ", fields.Select(f => $"self.{f.Name}"));

                    if (isServerFile)
                    {
                        string customConstructor = fields.Count() > 0 ? ServerCustomConstructLayout
                        .Replace("{NamespaceName}", nsName)
                        .Replace("{StructName}", structName)
                        .Replace("{ConstructorParams}", constParams)
                        .Replace("{ConstructorInitializers}", constInits) : "";

                        string renderedStruct = ServerStructLayout
                            .Replace("{NamespaceName}", nsName)
                            .Replace("{StructName}", structName)
                            .Replace("{FieldDeclarations}", fieldDecls)
                            .Replace("{CustomConstructor}", customConstructor)
                            .Replace("{SerializeFields}", serializeList);

                        structList.Add(renderedStruct);
                    }
                    else
                    {
                        string customConstructor = fields.Count() > 0 ? ClientCustomConstructLayout
                        .Replace("{StructName}", structName)
                        .Replace("{ConstructorParams}", constParams)
                        .Replace("{ConstructorInitializers}", constInits) : "";

                        string renderedStruct = ClientStructLayout
                            .Replace("{NamespaceName}", nsName)
                            .Replace("{StructName}", structName)
                            .Replace("{FieldDeclarations}", fieldDecls)
                            .Replace("{CustomConstructor}", customConstructor)
                            .Replace("{SerializeFields}", serializeList);

                        structList.Add(renderedStruct);
                    }
                }

                string finalCode = FileLayout
                    .Replace("{NamespaceName}", nsName)
                    .Replace("{EnumItems}", string.Join("\n", enumList))
                    .Replace("{Structs}", string.Join("\n", structList));

                File.WriteAllText(Path.Combine(outputPath, outputFileName), finalCode, Encoding.UTF8);
                Console.WriteLine($"[Packet] Generated: {outputFileName}");
            }
        }

        private string ToPascalCase(string input) => string.Join("", input.Split('_', ' ', '-').Select(s => s.Length > 0 ? char.ToUpper(s[0]) + s.Substring(1).ToLower() : ""));
        private string ToSnakeCase(string input) => Regex.Replace(input, @"(?<!^)([A-Z])", "_$1").ToLower();
    }
}
