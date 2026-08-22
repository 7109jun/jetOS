```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>Exe</OutputType>
    <TargetFramework>net8.0-windows</TargetFramework>
    <ImplicitUsings>enable</ImplicitUsings>
    <Nullable>disable</Nullable>
    <EnableWindowsTargeting>true</EnableWindowsTargeting>
    <AssemblyName>jash</AssemblyName>
    <RootNamespace>Jash</RootNamespace>
  </PropertyGroup>

  <ItemGroup>
    <Using Include="System.Diagnostics" />
    <Using Include="System.Text" />
    <Using Include="System.Net.Http" />
    <Using Include="System.Runtime.InteropServices" />
    <Using Include="System.Globalization" />
  </ItemGroup>
</Project>
```
```cs
namespace Jash;

public static class Program
{
    public static int Main(string[] args)
    {
        if (!OperatingSystem.IsWindows())
        {
            Console.Error.WriteLine("This build is Windows-only.");
            return 1;
        }

        try
        {
            Console.OutputEncoding = Encoding.UTF8;
            Console.InputEncoding = Encoding.UTF8;
        }
        catch
        {
        }

        Console.WriteLine("Jash ALL build");
        Console.WriteLine("commands: help, base64, mem, browse, exit");

        while (true)
        {
            Console.Write("Jash> ");
            string line = Console.ReadLine();

            if (line == null)
                break;

            line = line.Trim();

            if (line.Length == 0)
                continue;

            var argv = SplitArgs(line);

            if (argv.Count == 0)
                continue;

            string cmd = argv[0].ToLowerInvariant();

            try
            {
                switch (cmd)
                {
                    case "exit":
                    case "quit":
                        return 0;

                    case "help":
                        Help();
                        break;

                    case "base64":
                        Base64Command(argv);
                        break;

                    case "mem":
                    case "memory":
                        MemCommand(argv);
                        break;

                    case "browse":
                    case "browser":
                        BrowseCommand(argv);
                        break;

                    default:
                        Console.WriteLine($"unknown command: {cmd}");
                        Console.WriteLine("try: help");
                        break;
                }
            }
            catch (Exception e)
            {
                Console.WriteLine($"error: {e.Message}");
            }
        }

        return 0;
    }

    private static void Help()
    {
        Console.WriteLine("Jash ALL commands");
        Console.WriteLine();
        Console.WriteLine("base64 encode <text...>");
        Console.WriteLine("base64 decode <base64...>");
        Console.WriteLine("base64 encode-file <in> [out]");
        Console.WriteLine("base64 decode-file <in> [out]");
        Console.WriteLine("base64 download <url> [out.b64]");
        Console.WriteLine();
        Console.WriteLine("mem list");
        Console.WriteLine("mem view <pid> <address> [size]");
        Console.WriteLine("mem write <pid> <address> hex <bytes...>");
        Console.WriteLine("mem write <pid> <address> text <string...>");
        Console.WriteLine();
        Console.WriteLine("browse <url-or-file>");
        Console.WriteLine("  browser keys:");
        Console.WriteLine("  Ctrl+X exit");
        Console.WriteLine("  Up/Down/PageUp/PageDown scroll");
        Console.WriteLine("  Enter click hover element");
        Console.WriteLine("  B back, F forward, R reload");
        Console.WriteLine("  Mouse move/click supported");
    }

    private static List<string> SplitArgs(string line)
    {
        var result = new List<string>();
        var sb = new StringBuilder();
        bool inQuote = false;
        char quoteChar = '"';

        for (int i = 0; i < line.Length; i++)
        {
            char c = line[i];

            if (inQuote)
            {
                if (c == quoteChar)
                {
                    inQuote = false;
                }
                else
                {
                    sb.Append(c);
                }
            }
            else
            {
                if (c == '"' || c == '\'')
                {
                    inQuote = true;
                    quoteChar = c;
                }
                else if (char.IsWhiteSpace(c))
                {
                    if (sb.Length > 0)
                    {
                        result.Add(sb.ToString());
                        sb.Clear();
                    }
                }
                else
                {
                    sb.Append(c);
                }
            }
        }

        if (sb.Length > 0)
            result.Add(sb.ToString());

        return result;
    }

    private static void Base64Command(List<string> args)
    {
        if (args.Count < 2)
        {
            Help();
            return;
        }

        string sub = args[1].ToLowerInvariant();

        switch (sub)
        {
            case "encode":
            {
                string text = string.Join(" ", args.Skip(2));
                Console.WriteLine(Convert.ToBase64String(Encoding.UTF8.GetBytes(text)));
                break;
            }

            case "decode":
            {
                string b64 = CleanBase64(string.Join("", args.Skip(2)));
                byte[] bytes = Convert.FromBase64String(b64);
                Console.WriteLine(Encoding.UTF8.GetString(bytes));
                break;
            }

            case "encode-file":
            {
                if (args.Count < 3)
                    throw new Exception("usage: base64 encode-file <in> [out]");

                byte[] bytes = File.ReadAllBytes(args[2]);
                string b64 = Convert.ToBase64String(bytes);

                if (args.Count >= 4)
                {
                    File.WriteAllText(args[3], b64, Encoding.UTF8);
                    Console.WriteLine($"saved: {args[3]}");
                }
                else
                {
                    Console.WriteLine(b64);
                }

                break;
            }

            case "decode-file":
            {
                if (args.Count < 3)
                    throw new Exception("usage: base64 decode-file <in> [out]");

                string b64 = CleanBase64(File.ReadAllText(args[2], Encoding.UTF8));
                byte[] bytes = Convert.FromBase64String(b64);

                if (args.Count >= 4)
                {
                    File.WriteAllBytes(args[3], bytes);
                    Console.WriteLine($"saved: {args[3]}");
                }
                else
                {
                    Console.WriteLine(Encoding.UTF8.GetString(bytes));
                }

                break;
            }

            case "download":
            {
                if (args.Count < 3)
                    throw new Exception("usage: base64 download <url> [out.b64]");

                byte[] bytes = Web.FetchBytes(args[2]);
                string b64 = Convert.ToBase64String(bytes);

                if (args.Count >= 4)
                {
                    File.WriteAllText(args[3], b64, Encoding.UTF8);
                    Console.WriteLine($"saved: {args[3]}");
                }
                else
                {
                    Console.WriteLine(b64);
                }

                break;
            }

            default:
                throw new Exception($"unknown base64 subcommand: {sub}");
        }
    }

    private static string CleanBase64(string s)
    {
        if (string.IsNullOrEmpty(s))
            return "";

        return new string(s.Where(c => !char.IsWhiteSpace(c)).ToArray());
    }

    private static void MemCommand(List<string> args)
    {
        if (args.Count < 2)
        {
            Help();
            return;
        }

        string sub = args[1].ToLowerInvariant();

        switch (sub)
        {
            case "list":
            {
                foreach (var p in Process.GetProcesses().OrderBy(p => p.ProcessName).Take(200))
                    Console.WriteLine($"{p.Id,8} {p.ProcessName}");

                break;
            }

            case "view":
            {
                if (args.Count < 4)
                    throw new Exception("usage: mem view <pid> <address> [size]");

                int pid = int.Parse(args[2]);
                long addr = ParseAddress(args[3]);
                int size = args.Count >= 5 ? int.Parse(args[4]) : 256;
                size = Math.Clamp(size, 1, 4096);

                byte[] data = WinMemory.Read(pid, addr, size);
                WriteHexdump(addr, data);
                break;
            }

            case "write":
            {
                if (args.Count < 5)
                    throw new Exception("usage: mem write <pid> <address> hex <bytes...> | text <string...>");

                int pid = int.Parse(args[2]);
                long addr = ParseAddress(args[3]);
                string mode = args[4].ToLowerInvariant();

                byte[] bytes;

                if (mode == "text")
                {
                    string text = string.Join(" ", args.Skip(5));
                    bytes = Encoding.UTF8.GetBytes(text);
                }
                else if (mode == "hex")
                {
                    bytes = args.Skip(5).Select(ParseByte).ToArray();
                }
                else
                {
                    bytes = args.Skip(4).Select(ParseByte).ToArray();
                }

                WinMemory.Write(pid, addr, bytes);
                Console.WriteLine($"wrote {bytes.Length} byte(s) to pid {pid} address 0x{addr:X}");
                break;
            }

            default:
                throw new Exception($"unknown mem subcommand: {sub}");
        }
    }

    private static long ParseAddress(string s)
    {
        s = s.Trim();

        if (s.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
            return Convert.ToInt64(s.Substring(2), 16);

        bool hasHex = s.Any(c =>
            (c >= 'a' && c <= 'f') ||
            (c >= 'A' && c <= 'F'));

        if (hasHex)
            return Convert.ToInt64(s, 16);

        return long.Parse(s);
    }

    private static byte ParseByte(string s)
    {
        s = s.Trim();

        if (s.StartsWith("d:", StringComparison.OrdinalIgnoreCase))
            return byte.Parse(s.Substring(2));

        try
        {
            return Convert.ToByte(s, 16);
        }
        catch
        {
            return byte.Parse(s);
        }
    }

    private static void WriteHexdump(long baseAddr, byte[] data)
    {
        for (int off = 0; off < data.Length; off += 16)
        {
            var sb = new StringBuilder();

            sb.Append("0x");
            sb.Append((baseAddr + off).ToString("X"));
            sb.Append("  ");

            for (int i = 0; i < 16; i++)
            {
                if (off + i < data.Length)
                    sb.Append(data[off + i].ToString("X2"));
                else
                    sb.Append("  ");

                if (i == 7)
                    sb.Append(' ');

                sb.Append(' ');
            }

            sb.Append(" |");

            for (int i = 0; i < 16 && off + i < data.Length; i++)
            {
                byte b = data[off + i];
                sb.Append(b >= 32 && b < 127 ? (char)b : '.');
            }

            sb.Append('|');

            Console.WriteLine(sb.ToString());
        }
    }

    private static void BrowseCommand(List<string> args)
    {
        string target = args.Count >= 2 ? args[1] : "https://example.com";
        var browser = new AsciiBrowser();
        browser.Run(target);
    }
}

public static class Web
{
    private static readonly HttpClient Http = Create();

    private static HttpClient Create()
    {
        var client = new HttpClient(new HttpClientHandler
        {
            AllowAutoRedirect = true
        });

        client.Timeout = TimeSpan.FromSeconds(20);

        try
        {
            client.DefaultRequestHeaders.UserAgent.ParseAdd("Jash/1.0");
            client.DefaultRequestHeaders.Accept.ParseAdd("text/html,application/xhtml+xml,text/plain,*/*;q=0.8");
        }
        catch
        {
        }

        return client;
    }

    public static string FetchString(string url)
    {
        if (url.StartsWith("file://", StringComparison.OrdinalIgnoreCase))
        {
            string path = url.Substring(7).Replace('/', Path.DirectorySeparatorChar);
            return File.ReadAllText(path);
        }

        if (!url.StartsWith("http://", StringComparison.OrdinalIgnoreCase) &&
            !url.StartsWith("https://", StringComparison.OrdinalIgnoreCase))
        {
            if (File.Exists(url))
                return File.ReadAllText(url);

            url = "https://" + url;
        }

        using var resp = Http.GetAsync(url).GetAwaiter().GetResult();
        resp.EnsureSuccessStatusCode();

        string text = resp.Content.ReadAsStringAsync().GetAwaiter().GetResult();

        if (text.Length > 2_000_000)
            text = text.Substring(0, 2_000_000);

        return text;
    }

    public static byte[] FetchBytes(string url)
    {
        if (!url.StartsWith("http://", StringComparison.OrdinalIgnoreCase) &&
            !url.StartsWith("https://", StringComparison.OrdinalIgnoreCase))
        {
            if (File.Exists(url))
                return File.ReadAllBytes(url);

            url = "https://" + url;
        }

        using var resp = Http.GetAsync(url).GetAwaiter().GetResult();
        resp.EnsureSuccessStatusCode();
        return resp.Content.ReadAsByteArrayAsync().GetAwaiter().GetResult();
    }

    public static string ResolveUrl(string baseUrl, string href)
    {
        if (string.IsNullOrWhiteSpace(href))
            return baseUrl;

        if (Uri.TryCreate(href, UriKind.Absolute, out var abs))
            return abs.AbsoluteUri;

        if (Uri.TryCreate(baseUrl, UriKind.Absolute, out var baseUri))
        {
            if (Uri.TryCreate(baseUri, href, out var result))
                return result.AbsoluteUri;
        }

        return href;
    }
}

public static class WinMemory
{
    private const uint PROCESS_VM_READ = 0x0010;
    private const uint PROCESS_VM_WRITE = 0x0020;
    private const uint PROCESS_VM_OPERATION = 0x0008;
    private const uint PROCESS_QUERY_INFORMATION = 0x0400;

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr OpenProcess(uint dwDesiredAccess, bool bInheritHandle, int dwProcessId);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool ReadProcessMemory(
        IntPtr hProcess,
        IntPtr lpBaseAddress,
        [Out] byte[] lpBuffer,
        int dwSize,
        out UIntPtr lpNumberOfBytesRead);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool WriteProcessMemory(
        IntPtr hProcess,
        IntPtr lpBaseAddress,
        byte[] lpBuffer,
        int dwSize,
        out UIntPtr lpNumberOfBytesWritten);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool CloseHandle(IntPtr hObject);

    public static byte[] Read(int pid, long address, int size)
    {
        IntPtr handle = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, false, pid);

        if (handle == IntPtr.Zero)
            throw new Exception($"OpenProcess failed for pid {pid} (Win32 error {Marshal.GetLastWin32Error()})");

        try
        {
            byte[] buffer = new byte[size];

            bool ok = ReadProcessMemory(handle, new IntPtr(address), buffer, size, out UIntPtr read);

            if (!ok)
                throw new Exception($"ReadProcessMemory failed (Win32 error {Marshal.GetLastWin32Error()})");

            ulong readBytes = read.ToUInt64();

            if (readBytes < (ulong)size)
                Array.Resize(ref buffer, (int)readBytes);

            return buffer;
        }
        finally
        {
            CloseHandle(handle);
        }
    }

    public static void Write(int pid, long address, byte[] data)
    {
        IntPtr handle = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, false, pid);

        if (handle == IntPtr.Zero)
            throw new Exception($"OpenProcess failed for pid {pid} (Win32 error {Marshal.GetLastWin32Error()})");

        try
        {
            bool ok = WriteProcessMemory(handle, new IntPtr(address), data, data.Length, out UIntPtr written);

            if (!ok)
                throw new Exception($"WriteProcessMemory failed (Win32 error {Marshal.GetLastWin32Error()})");

            if (written.ToUInt64() != (ulong)data.Length)
                throw new Exception("partial write");
        }
        finally
        {
            CloseHandle(handle);
        }
    }
}

public class DomNode
{
    private static int nextId = 1;

    public int Id;
    public string Tag = "";
    public string Text = "";
    public DomNode Parent;
    public List<DomNode> Children = new();
    public Dictionary<string, string> Attrs = new(StringComparer.OrdinalIgnoreCase);
    public Dictionary<string, string> InlineStyle = new(StringComparer.OrdinalIgnoreCase);
    public Dictionary<string, string> ComputedStyle = new(StringComparer.OrdinalIgnoreCase);
    public List<object> ClickListeners = new();
    public string OnClickSource;

    public DomNode()
    {
        Id = nextId++;
    }

    public bool IsText => Tag == "#text";
}

public static class HtmlParser
{
    private static readonly HashSet<string> VoidTags = new(StringComparer.OrdinalIgnoreCase)
    {
        "area", "base", "br", "col", "embed", "hr", "img", "input",
        "link", "meta", "source", "track", "wbr"
    };

    public static DomNode Parse(string html)
    {
        var root = new DomNode { Tag = "html" };
        var stack = new Stack<DomNode>();
        stack.Push(root);

        int i = 0;

        while (i < html.Length)
        {
            if (html[i] == '<')
            {
                if (i + 4 < html.Length && html.Substring(i, 4).Equals("<!--", StringComparison.Ordinal))
                {
                    int close = html.IndexOf("-->", i + 4, StringComparison.OrdinalIgnoreCase);
                    i = close < 0 ? html.Length : close + 3;
                    continue;
                }

                int closeTag = html.IndexOf('>', i);

                if (closeTag < 0)
                    break;

                string tagContent = html.Substring(i + 1, closeTag - i - 1).Trim();

                if (tagContent.StartsWith("!", StringComparison.Ordinal))
                {
                    i = closeTag + 1;
                    continue;
                }

                if (tagContent.StartsWith("/", StringComparison.Ordinal))
                {
                    string closeName = tagContent.Substring(1).Trim().Split(' ')[0].ToLowerInvariant();
                    PopUntil(stack, closeName);
                    i = closeTag + 1;
                    continue;
                }

                string tagName = GetTagName(tagContent).ToLowerInvariant();

                if (tagName == "script" || tagName == "style")
                {
                    string closeMarker = "</" + tagName;
                    int end = html.IndexOf(closeMarker, closeTag + 1, StringComparison.OrdinalIgnoreCase);

                    if (end < 0)
                        end = html.Length;

                    int gt = html.IndexOf('>', end);
                    string raw = html.Substring(closeTag + 1, end - closeTag - 1);

                    var node = new DomNode
                    {
                        Tag = tagName,
                        Text = raw,
                        Parent = stack.Peek()
                    };

                    node.Attrs = ParseAttributes(tagContent);
                    stack.Peek().Children.Add(node);

                    i = gt < 0 ? html.Length : gt + 1;
                    continue;
                }

                var element = new DomNode
                {
                    Tag = tagName,
                    Parent = stack.Peek()
                };

                element.Attrs = ParseAttributes(tagContent);

                if (element.Attrs.TryGetValue("style", out string styleAttr))
                {
                    element.InlineStyle = CssParser.ParseDeclarations(styleAttr);
                }

                if (element.Attrs.TryGetValue("onclick", out string onclick))
                {
                    element.OnClickSource = onclick;
                }

                stack.Peek().Children.Add(element);

                bool selfClosing = tagContent.EndsWith("/") || VoidTags.Contains(tagName);

                if (!selfClosing)
                    stack.Push(element);

                i = closeTag + 1;
            }
            else
            {
                int next = html.IndexOf('<', i);

                if (next < 0)
                    next = html.Length;

                string text = html.Substring(i, next - i);

                if (!string.IsNullOrWhiteSpace(text))
                {
                    var textNode = new DomNode
                    {
                        Tag = "#text",
                        Text = DecodeEntities(text),
                        Parent = stack.Peek()
                    };

                    stack.Peek().Children.Add(textNode);
                }

                i = next;
            }
        }

        return root;
    }

    private static void PopUntil(Stack<DomNode> stack, string tagName)
    {
        if (stack.Count <= 1)
            return;

        var temp = new List<DomNode>();

        while (stack.Count > 1)
        {
            var top = stack.Pop();

            if (top.Tag.Equals(tagName, StringComparison.OrdinalIgnoreCase))
                return;

            temp.Add(top);
        }

        for (int i = temp.Count - 1; i >= 0; i--)
            stack.Push(temp[i]);
    }

    private static string GetTagName(string tagContent)
    {
        int i = 0;

        while (i < tagContent.Length && !char.IsWhiteSpace(tagContent[i]) && tagContent[i] != '/')
            i++;

        return tagContent.Substring(0, i);
    }

    private static Dictionary<string, string> ParseAttributes(string tagContent)
    {
        var attrs = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

        int i = 0;

        while (i < tagContent.Length && !char.IsWhiteSpace(tagContent[i]) && tagContent[i] != '/')
            i++;

        while (i < tagContent.Length)
        {
            while (i < tagContent.Length && (char.IsWhiteSpace(tagContent[i]) || tagContent[i] == '/'))
                i++;

            if (i >= tagContent.Length)
                break;

            int nameStart = i;

            while (i < tagContent.Length && tagContent[i] != '=' && !char.IsWhiteSpace(tagContent[i]) && tagContent[i] != '/')
                i++;

            string name = tagContent.Substring(nameStart, i - nameStart);

            if (name.Length == 0)
                break;

            while (i < tagContent.Length && char.IsWhiteSpace(tagContent[i]))
                i++;

            if (i >= tagContent.Length)
            {
                attrs[name] = "";
                break;
            }

            if (tagContent[i] != '=')
            {
                attrs[name] = "";
                continue;
            }

            i++; // =

            while (i < tagContent.Length && char.IsWhiteSpace(tagContent[i]))
                i++;

            if (i >= tagContent.Length)
            {
                attrs[name] = "";
                break;
            }

            char quote = tagContent[i];

            if (quote == '"' || quote == '\'')
            {
                i++;
                int valueStart = i;

                while (i < tagContent.Length && tagContent[i] != quote)
                    i++;

                string value = tagContent.Substring(valueStart, i - valueStart);
                attrs[name] = DecodeEntities(value);

                if (i < tagContent.Length)
                    i++;
            }
            else
            {
                int valueStart = i;

                while (i < tagContent.Length && !char.IsWhiteSpace(tagContent[i]) && tagContent[i] != '/')
                    i++;

                string value = tagContent.Substring(valueStart, i - valueStart);
                attrs[name] = DecodeEntities(value);
            }
        }

        return attrs;
    }

    public static string DecodeEntities(string s)
    {
        if (string.IsNullOrEmpty(s) || s.IndexOf('&') < 0)
            return s;

        s = s
            .Replace("&nbsp;", " ")
            .Replace("&amp;", "&")
            .Replace("&lt;", "<")
            .Replace("&gt;", ">")
            .Replace("&quot;", "\"")
            .Replace("&#39;", "'")
            .Replace("&apos;", "'");

        var sb = new StringBuilder();

        for (int i = 0; i < s.Length; i++)
        {
            if (s[i] == '&' && i + 1 < s.Length && s[i + 1] == '#')
            {
                int semi = s.IndexOf(';', i + 2);

                if (semi > i + 2)
                {
                    string num = s.Substring(i + 2, semi - i - 2);
                    int code = 0;
                    bool ok = false;

                    if (num.StartsWith("x", StringComparison.OrdinalIgnoreCase))
                        ok = int.TryParse(num.Substring(1), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out code);
                    else
                        ok = int.TryParse(num, NumberStyles.Integer, CultureInfo.InvariantCulture, out code);

                    if (ok && code > 0 && code <= 0x10FFFF)
                    {
                        try
                        {
                            sb.Append(char.ConvertFromUtf32(code));
                            i = semi;
                            continue;
                        }
                        catch
                        {
                        }
                    }
                }
            }

            sb.Append(s[i]);
        }

        return sb.ToString();
    }
}

public class CssRule
{
    public string Selector = "";
    public Dictionary<string, string> Declarations = new(StringComparer.OrdinalIgnoreCase);
}

public static class CssParser
{
    public static List<CssRule> Parse(string css)
    {
        css = RemoveComments(css ?? "");
        var rules = new List<CssRule>();

        int i = 0;

        while (i < css.Length)
        {
            int brace = css.IndexOf('{', i);

            if (brace < 0)
                break;

            string selector = css.Substring(i, brace - i).Trim();

            int end = css.IndexOf('}', brace);

            if (end < 0)
                break;

            string body = css.Substring(brace + 1, end - brace - 1);

            foreach (string sel in selector.Split(','))
            {
                string trimmed = sel.Trim();

                if (trimmed.Length == 0)
                    continue;

                rules.Add(new CssRule
                {
                    Selector = trimmed,
                    Declarations = ParseDeclarations(body)
                });
            }

            i = end + 1;
        }

        return rules;
    }

    public static Dictionary<string, string> ParseDeclarations(string body)
    {
        var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

        foreach (string part in body.Split(';', StringSplitOptions.RemoveEmptyEntries))
        {
            int idx = part.IndexOf(':');

            if (idx <= 0)
                continue;

            string name = part.Substring(0, idx).Trim();
            string value = part.Substring(idx + 1).Trim();

            if (name.Length > 0)
                result[name] = value;
        }

        return result;
    }

    private static string RemoveComments(string css)
    {
        var sb = new StringBuilder();

        int i = 0;

        while (i < css.Length)
        {
            if (i + 1 < css.Length && css[i] == '/' && css[i + 1] == '*')
            {
                int end = css.IndexOf("*/", i + 2, StringComparison.Ordinal);

                if (end < 0)
                    break;

                i = end + 2;
                continue;
            }

            sb.Append(css[i]);
            i++;
        }

        return sb.ToString();
    }
}

public static class StyleEngine
{
    public static void ComputeAll(DomNode root, List<CssRule> rules)
    {
        var inherit = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
        {
            ["color"] = "gray",
            ["background-color"] = "black"
        };

        Compute(root, rules, inherit);
    }

    private static void Compute(DomNode node, List<CssRule> rules, Dictionary<string, string> inherit)
    {
        if (node.IsText)
        {
            node.ComputedStyle = new Dictionary<string, string>(inherit, StringComparer.OrdinalIgnoreCase);
            return;
        }

        var computed = new Dictionary<string, string>(inherit, StringComparer.OrdinalIgnoreCase);

        ApplyDefaults(node.Tag, computed);

        foreach (var rule in rules)
        {
            if (Matches(node, rule.Selector))
                ApplyDeclarations(rule.Declarations, computed);
        }

        ApplyDeclarations(node.InlineStyle, computed);

        node.ComputedStyle = computed;

        var childInherit = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
        {
            ["color"] = computed.TryGetValue("color", out var c) ? c : "gray",
            ["background-color"] = computed.TryGetValue("background-color", out var bg) ? bg : "black"
        };

        foreach (var child in node.Children)
            Compute(child, rules, childInherit);
    }

    private static void ApplyDefaults(string tag, Dictionary<string, string> style)
    {
        tag = tag.ToLowerInvariant();

        style["display"] = tag switch
        {
            "address" or "article" or "aside" or "body" or "div" or "footer" or
            "form" or "h1" or "h2" or "h3" or "h4" or "h5" or "h6" or
            "header" or "html" or "li" or "nav" or "p" or "pre" or
            "section" or "table" or "ul" or "ol" => "block",

            "script" or "style" or "head" or "title" or "meta" or "link" => "none",

            _ => "inline"
        };

        if (tag is "h1" or "h2" or "h3" or "h4" or "h5" or "h6")
            style["color"] = "yellow";

        if (tag == "a")
            style["color"] = "cyan";

        if (tag == "button")
            style["color"] = "green";

        if (tag == "body")
            style["color"] = "gray";
    }

    private static void ApplyDeclarations(Dictionary<string, string> declarations, Dictionary<string, string> target)
    {
        foreach (var kv in declarations)
            target[kv.Key] = kv.Value;
    }

    public static bool Matches(DomNode node, string selector)
    {
        if (node == null || node.IsText)
            return false;

        selector = selector.Trim();

        if (selector.Length == 0)
            return false;

        if (selector == "*")
            return true;

        var parts = selector.Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);

        if (parts.Length == 0)
            return false;

        if (!MatchCompound(node, parts[^1]))
            return false;

        return MatchAncestors(node.Parent, parts, parts.Length - 2);
    }

    private static bool MatchAncestors(DomNode node, string[] parts, int idx)
    {
        if (idx < 0)
            return true;

        for (var p = node; p != null; p = p.Parent)
        {
            if (MatchCompound(p, parts[idx]) && MatchAncestors(p.Parent, parts, idx - 1))
                return true;
        }

        return false;
    }

    private static bool MatchCompound(DomNode node, string compound)
    {
        if (node == null || node.IsText)
            return false;

        string tag = null;
        string id = null;
        var classes = new List<string>();

        int i = 0;
        var current = new StringBuilder();

        char mode = 't';

        while (i <= compound.Length)
        {
            char c = i < compound.Length ? compound[i] : '\0';

            if (i == compound.Length || c == '#' || c == '.')
            {
                string value = current.ToString();
                current.Clear();

                if (mode == 't' && value.Length > 0)
                    tag = value;
                else if (mode == '#' && value.Length > 0)
                    id = value;
                else if (mode == '.' && value.Length > 0)
                    classes.Add(value);

                if (i < compound.Length)
                    mode = c == '#' ? 'i' : 'c';
            }
            else
            {
                current.Append(c);
            }

            i++;
        }

        if (tag != null && !node.Tag.Equals(tag, StringComparison.OrdinalIgnoreCase))
            return false;

        if (id != null)
        {
            if (!node.Attrs.TryGetValue("id", out string nodeId) || !nodeId.Equals(id, StringComparison.OrdinalIgnoreCase))
                return false;
        }

        if (classes.Count > 0)
        {
            string classAttr = node.Attrs.TryGetValue("class", out string c) ? c : "";
            var nodeClasses = classAttr.Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);

            foreach (string wanted in classes)
            {
                bool found = nodeClasses.Any(x => x.Equals(wanted, StringComparison.OrdinalIgnoreCase));

                if (!found)
                    return false;
            }
        }

        return true;
    }

    public static DomNode FirstMatch(DomNode root, string selector)
    {
        if (root == null)
            return null;

        if (!root.IsText && Matches(root, selector))
            return root;

        foreach (var child in root.Children)
        {
            var found = FirstMatch(child, selector);

            if (found != null)
                return found;
        }

        return null;
    }

    public static List<DomNode> AllMatches(DomNode root, string selector)
    {
        var result = new List<DomNode>();
        CollectMatches(root, selector, result);
        return result;
    }

    private static void CollectMatches(DomNode node, string selector, List<DomNode> result)
    {
        if (node == null)
            return;

        if (!node.IsText && Matches(node, selector))
            result.Add(node);

        foreach (var child in node.Children)
            CollectMatches(child, selector, result);
    }

    public static DomNode FindById(DomNode root, string id)
    {
        if (root == null)
            return null;

        if (!root.IsText && root.Attrs.TryGetValue("id", out string nodeId) && nodeId.Equals(id, StringComparison.OrdinalIgnoreCase))
            return root;

        foreach (var child in root.Children)
        {
            var found = FindById(child, id);

            if (found != null)
                return found;
        }

        return null;
    }

    public static ConsoleColor ParseColor(string value, ConsoleColor fallback)
    {
        if (string.IsNullOrWhiteSpace(value))
            return fallback;

        value = value.Trim().ToLowerInvariant();

        return value switch
        {
            "black" => ConsoleColor.Black,
            "red" => ConsoleColor.Red,
            "green" => ConsoleColor.Green,
            "yellow" => ConsoleColor.Yellow,
            "blue" => ConsoleColor.Blue,
            "magenta" => ConsoleColor.Magenta,
            "cyan" => ConsoleColor.Cyan,
            "white" => ConsoleColor.White,
            "gray" or "grey" => ConsoleColor.Gray,
            "darkred" => ConsoleColor.DarkRed,
            "darkgreen" => ConsoleColor.DarkGreen,
            "darkyellow" => ConsoleColor.DarkYellow,
            "darkblue" => ConsoleColor.DarkBlue,
            "darkmagenta" => ConsoleColor.DarkMagenta,
            "darkcyan" => ConsoleColor.DarkCyan,
            "darkgray" or "darkgrey" => ConsoleColor.DarkGray,
            _ => fallback
        };
    }

    public static int GetInt(Dictionary<string, string> style, string name)
    {
        if (!style.TryGetValue(name, out string value))
            return 0;

        value = value.Trim().ToLowerInvariant().Replace("px", "").Trim();

        if (int.TryParse(value, out int n))
            return n;

        return 0;
    }
}

public class JsUndefined
{
    public static readonly JsUndefined Value = new();

    public override string ToString()
    {
        return "undefined";
    }
}

public class JsNative
{
    public Func<List<object>, object> Fn;
}

public class JsFunction
{
    public string Name;
    public List<string> Params = new();
    public JsBlock Body;
    public Scope Closure;
}

public class Scope
{
    public Scope Parent;
    public Dictionary<string, object> Vars = new(StringComparer.Ordinal);

    public Scope(Scope parent)
    {
        Parent = parent;
    }

    public bool TryGet(string name, out object value)
    {
        for (var s = this; s != null; s = s.Parent)
        {
            if (s.Vars.TryGetValue(name, out value))
                return true;
        }

        value = null;
        return false;
    }

    public void Define(string name, object value)
    {
        Vars[name] = value;
    }

    public void Set(string name, object value)
    {
        for (var s = this; s != null; s = s.Parent)
        {
            if (s.Vars.ContainsKey(name))
            {
                s.Vars[name] = value;
                return;
            }
        }

        Vars[name] = value;
    }
}

public abstract class JsStmt
{
}

public abstract class JsExpr
{
}

public class JsBlock : JsStmt
{
    public List<JsStmt> Body = new();
}

public class JsVarDecl : JsStmt
{
    public string Name;
    public JsExpr Init;
}

public class JsExprStmt : JsStmt
{
    public JsExpr Expr;
}

public class JsIf : JsStmt
{
    public JsExpr Cond;
    public JsStmt Then;
    public JsStmt Else;
}

public class JsWhile : JsStmt
{
    public JsExpr Cond;
    public JsStmt Body;
}

public class JsFor : JsStmt
{
    public JsStmt Init;
    public JsExpr Cond;
    public JsExpr Update;
    public JsStmt Body;
}

public class JsReturn : JsStmt
{
    public JsExpr Expr;
}

public class JsBreak : JsStmt
{
}

public class JsContinue : JsStmt
{
}

public class JsFunctionDecl : JsStmt
{
    public string Name;
    public List<string> Params = new();
    public JsBlock Body;
}

public class JsNumber : JsExpr
{
    public double Value;
}

public class JsString : JsExpr
{
    public string Value;
}

public class JsBool : JsExpr
{
    public bool Value;
}

public class JsNull : JsExpr
{
}

public class JsUndefinedExpr : JsExpr
{
}

public class JsIdentifier : JsExpr
{
    public string Name;
}

public class JsAssign : JsExpr
{
    public JsExpr Target;
    public string Op;
    public JsExpr Value;
}

public class JsBinary : JsExpr
{
    public string Op;
    public JsExpr Left;
    public JsExpr Right;
}

public class JsLogical : JsExpr
{
    public string Op;
    public JsExpr Left;
    public JsExpr Right;
}

public class JsUnary : JsExpr
{
    public string Op;
    public JsExpr Operand;
}

public class JsCall : JsExpr
{
    public JsExpr Callee;
    public List<JsExpr> Args = new();
}

public class JsMember : JsExpr
{
    public JsExpr Obj;
    public string Prop;
}

public class JsIndex : JsExpr
{
    public JsExpr Obj;
    public JsExpr Index;
}

public class JsObjectLit : JsExpr
{
    public List<(string Key, JsExpr Value)> Props = new();
}

public class JsArrayLit : JsExpr
{
    public List<JsExpr> Items = new();
}

public class JsFunctionExpr : JsExpr
{
    public string Name;
    public List<string> Params = new();
    public JsBlock Body;
}

public enum JsTokenType
{
    EOF,
    Number,
    String,
    Ident,
    Keyword,
    Punct
}

public class JsToken
{
    public JsTokenType Type;
    public string Text;
    public double Number;
}

public class JsLexer
{
    private static readonly HashSet<string> Keywords = new()
    {
        "var", "let", "const", "function", "return", "if", "else",
        "while", "for", "break", "continue", "true", "false",
        "null", "undefined"
    };

    private readonly string src;
    private int pos;

    public JsLexer(string source)
    {
        src = source ?? "";
    }

    public List<JsToken> Lex()
    {
        var tokens = new List<JsToken>();

        while (true)
        {
            SkipTrivia();

            if (pos >= src.Length)
            {
                tokens.Add(new JsToken { Type = JsTokenType.EOF, Text = "" });
                break;
            }

            char c = src[pos];

            if (char.IsDigit(c) || (c == '.' && pos + 1 < src.Length && char.IsDigit(src[pos + 1])))
            {
                tokens.Add(ReadNumber());
                continue;
            }

            if (c == '"' || c == '\'')
            {
                tokens.Add(ReadString());
                continue;
            }

            if (char.IsLetter(c) || c == '_' || c == '$')
            {
                tokens.Add(ReadIdentOrKeyword());
                continue;
            }

            string three = pos + 2 < src.Length ? src.Substring(pos, 3) : "";
            string two = pos + 1 < src.Length ? src.Substring(pos, 2) : "";

            if (three is "===" or "!==")
            {
                tokens.Add(Punct(three));
                pos += 3;
                continue;
            }

            if (two is "==" or "!=" or "<=" or ">=" or "&&" or "||" or "+=" or "-=")
            {
                tokens.Add(Punct(two));
                pos += 2;
                continue;
            }

            tokens.Add(Punct(c.ToString()));
            pos++;
        }

        return tokens;
    }

    private JsToken Punct(string text)
    {
        return new JsToken { Type = JsTokenType.Punct, Text = text };
    }

    private void SkipTrivia()
    {
        while (pos < src.Length)
        {
            char c = src[pos];

            if (char.IsWhiteSpace(c))
            {
                pos++;
                continue;
            }

            if (c == '/' && pos + 1 < src.Length && src[pos + 1] == '/')
            {
                while (pos < src.Length && src[pos] != '\n')
                    pos++;

                continue;
            }

            if (c == '/' && pos + 1 < src.Length && src[pos + 1] == '*')
            {
                int end = src.IndexOf("*/", pos + 2, StringComparison.Ordinal);

                if (end < 0)
                {
                    pos = src.Length;
                    return;
                }

                pos = end + 2;
                continue;
            }

            break;
        }
    }

    private JsToken ReadNumber()
    {
        int start = pos;

        while (pos < src.Length && (char.IsDigit(src[pos]) || src[pos] == '.'))
            pos++;

        string text = src.Substring(start, pos - start);
        double.TryParse(text, NumberStyles.Any, CultureInfo.InvariantCulture, out double value);

        return new JsToken
        {
            Type = JsTokenType.Number,
            Text = text,
            Number = value
        };
    }

    private JsToken ReadString()
    {
        char quote = src[pos];
        pos++;

        var sb = new StringBuilder();

        while (pos < src.Length && src[pos] != quote)
        {
            if (src[pos] == '\\' && pos + 1 < src.Length)
            {
                pos++;

                switch (src[pos])
                {
                    case 'n': sb.Append('\n'); break;
                    case 't': sb.Append('\t'); break;
                    case 'r': sb.Append('\r'); break;
                    case '\\': sb.Append('\\'); break;
                    case '\'': sb.Append('\''); break;
                    case '"': sb.Append('"'); break;
                    default: sb.Append(src[pos]); break;
                }

                pos++;
            }
            else
            {
                sb.Append(src[pos]);
                pos++;
            }
        }

        if (pos < src.Length)
            pos++;

        return new JsToken
        {
            Type = JsTokenType.String,
            Text = sb.ToString()
        };
    }

    private JsToken ReadIdentOrKeyword()
    {
        int start = pos;

        while (pos < src.Length && (char.IsLetterOrDigit(src[pos]) || src[pos] == '_' || src[pos] == '$'))
            pos++;

        string text = src.Substring(start, pos - start);

        return new JsToken
        {
            Type = Keywords.Contains(text) ? JsTokenType.Keyword : JsTokenType.Ident,
            Text = text
        };
    }
}

public class JsParser
{
    private readonly List<JsToken> tokens;
    private int pos;

    public JsParser(string source)
    {
        tokens = new JsLexer(source).Lex();
    }

    private JsToken Current => pos < tokens.Count ? tokens[pos] : tokens[^1];

    private JsToken Peek(int offset = 1)
    {
        int idx = pos + offset;
        return idx < tokens.Count ? tokens[idx] : tokens[^1];
    }

    private JsToken Advance()
    {
        var t = Current;

        if (pos < tokens.Count - 1)
            pos++;

        return t;
    }

    private bool CheckPunct(string text)
    {
        return Current.Type == JsTokenType.Punct && Current.Text == text;
    }

    private bool CheckKeyword(string text)
    {
        return Current.Type == JsTokenType.Keyword && Current.Text == text;
    }

    private bool MatchPunct(string text)
    {
        if (!CheckPunct(text))
            return false;

        Advance();
        return true;
    }

    private bool MatchKeyword(string text)
    {
        if (!CheckKeyword(text))
            return false;

        Advance();
        return true;
    }

    private JsToken ExpectPunct(string text, string message)
    {
        if (CheckPunct(text))
            return Advance();

        throw new Exception($"{message} near '{Current.Text}'");
    }

    public List<JsStmt> ParseProgram()
    {
        var stmts = new List<JsStmt>();

        while (Current.Type != JsTokenType.EOF)
            stmts.Add(ParseStatement());

        return stmts;
    }

    private JsStmt ParseStatement()
    {
        if (CheckPunct("{"))
            return ParseBlock();

        if (CheckKeyword("var") || CheckKeyword("let") || CheckKeyword("const"))
            return ParseVarDecl(true);

        if (CheckKeyword("if"))
            return ParseIf();

        if (CheckKeyword("while"))
            return ParseWhile();

        if (CheckKeyword("for"))
            return ParseFor();

        if (CheckKeyword("function"))
            return ParseFunctionDecl();

        if (CheckKeyword("return"))
        {
            Advance();

            JsExpr expr = null;

            if (!CheckPunct(";") && !CheckPunct("}") && Current.Type != JsTokenType.EOF)
                expr = ParseExpression();

            MatchPunct(";");

            return new JsReturn { Expr = expr };
        }

        if (CheckKeyword("break"))
        {
            Advance();
            MatchPunct(";");
            return new JsBreak();
        }

        if (CheckKeyword("continue"))
        {
            Advance();
            MatchPunct(";");
            return new JsContinue();
        }

        if (MatchPunct(";"))
            return new JsBlock();

        var exprStmt = ParseExpression();
        MatchPunct(";");

        return new JsExprStmt { Expr = exprStmt };
    }

    private JsBlock ParseBlock()
    {
        ExpectPunct("{", "expected '{'");

        var block = new JsBlock();

        while (!CheckPunct("}") && Current.Type != JsTokenType.EOF)
            block.Body.Add(ParseStatement());

        ExpectPunct("}", "expected '}'");

        return block;
    }

    private JsVarDecl ParseVarDecl(bool consumeSemicolon)
    {
        Advance(); // var/let/const

        var name = ExpectIdent("expected variable name");

        JsExpr init = null;

        if (MatchPunct("="))
            init = ParseExpression();

        if (consumeSemicolon)
            MatchPunct(";");

        return new JsVarDecl
        {
            Name = name.Text,
            Init = init
        };
    }

    private JsToken ExpectIdent(string message)
    {
        if (Current.Type == JsTokenType.Ident || Current.Type == JsTokenType.Keyword)
            return Advance();

        throw new Exception($"{message} near '{Current.Text}'");
    }

    private JsIf ParseIf()
    {
        Advance(); // if
        ExpectPunct("(", "expected '(' after if");

        var cond = ParseExpression();

        ExpectPunct(")", "expected ')' after if condition");

        var then = ParseStatement();
        JsStmt els = null;

        if (MatchKeyword("else"))
            els = ParseStatement();

        return new JsIf
        {
            Cond = cond,
            Then = then,
            Else = els
        };
    }

    private JsWhile ParseWhile()
    {
        Advance(); // while
        ExpectPunct("(", "expected '(' after while");

        var cond = ParseExpression();

        ExpectPunct(")", "expected ')' after while condition");

        var body = ParseStatement();

        return new JsWhile
        {
            Cond = cond,
            Body = body
        };
    }

    private JsFor ParseFor()
    {
        Advance(); // for
        ExpectPunct("(", "expected '(' after for");

        JsStmt init = null;

        if (!CheckPunct(";"))
        {
            if (CheckKeyword("var") || CheckKeyword("let") || CheckKeyword("const"))
                init = ParseVarDecl(false);
            else
                init = new JsExprStmt { Expr = ParseExpression() };
        }

        ExpectPunct(";", "expected ';' in for");

        JsExpr cond = null;

        if (!CheckPunct(";"))
            cond = ParseExpression();

        ExpectPunct(";", "expected ';' in for");

        JsExpr update = null;

        if (!CheckPunct(")"))
            update = ParseExpression();

        ExpectPunct(")", "expected ')' in for");

        var body = ParseStatement();

        return new JsFor
        {
            Init = init,
            Cond = cond,
            Update = update,
            Body = body
        };
    }

    private JsFunctionDecl ParseFunctionDecl()
    {
        Advance(); // function

        var name = ExpectIdent("expected function name");

        ExpectPunct("(", "expected '(' after function name");

        var pars = ParseParameters();

        ExpectPunct(")", "expected ')' after function parameters");

        var body = ParseBlock();

        return new JsFunctionDecl
        {
            Name = name.Text,
            Params = pars,
            Body = body
        };
    }

    private List<string> ParseParameters()
    {
        var pars = new List<string>();

        if (CheckPunct(")"))
            return pars;

        while (true)
        {
            var p = ExpectIdent("expected parameter name");
            pars.Add(p.Text);

            if (!MatchPunct(","))
                break;
        }

        return pars;
    }

    private JsExpr ParseExpression()
    {
        return ParseAssignment();
    }

    private JsExpr ParseAssignment()
    {
        var left = ParseLogicalOr();

        if (CheckPunct("=") || CheckPunct("+=") || CheckPunct("-="))
        {
            string op = Advance().Text;
            var right = ParseAssignment();

            return new JsAssign
            {
                Target = left,
                Op = op,
                Value = right
            };
        }

        return left;
    }

    private JsExpr ParseLogicalOr()
    {
        var left = ParseLogicalAnd();

        while (CheckPunct("||"))
        {
            Advance();
            var right = ParseLogicalAnd();

            left = new JsLogical
            {
                Op = "||",
                Left = left,
                Right = right
            };
        }

        return left;
    }

    private JsExpr ParseLogicalAnd()
    {
        var left = ParseEquality();

        while (CheckPunct("&&"))
        {
            Advance();
            var right = ParseEquality();

            left = new JsLogical
            {
                Op = "&&",
                Left = left,
                Right = right
            };
        }

        return left;
    }

    private JsExpr ParseEquality()
    {
        var left = ParseRelational();

        while (CheckPunct("==") || CheckPunct("!=") || CheckPunct("===") || CheckPunct("!=="))
        {
            string op = Advance().Text;
            var right = ParseRelational();

            left = new JsBinary
            {
                Op = op,
                Left = left,
                Right = right
            };
        }

        return left;
    }

    private JsExpr ParseRelational()
    {
        var left = ParseAdditive();

        while (CheckPunct("<") || CheckPunct("<=") || CheckPunct(">") || CheckPunct(">="))
        {
            string op = Advance().Text;
            var right = ParseAdditive();

            left = new JsBinary
            {
                Op = op,
                Left = left,
                Right = right
            };
        }

        return left;
    }

    private JsExpr ParseAdditive()
    {
        var left = ParseMultiplicative();

        while (CheckPunct("+") || CheckPunct("-"))
        {
            string op = Advance().Text;
            var right = ParseMultiplicative();

            left = new JsBinary
            {
                Op = op,
                Left = left,
                Right = right
            };
        }

        return left;
    }

    private JsExpr ParseMultiplicative()
    {
        var left = ParseUnary();

        while (CheckPunct("*") || CheckPunct("/") || CheckPunct("%"))
        {
            string op = Advance().Text;
            var right = ParseUnary();

            left = new JsBinary
            {
                Op = op,
                Left = left,
                Right = right
            };
        }

        return left;
    }

    private JsExpr ParseUnary()
    {
        if (CheckPunct("!") || CheckPunct("-") || CheckPunct("+"))
        {
            string op = Advance().Text;
            var operand = ParseUnary();

            return new JsUnary
            {
                Op = op,
                Operand = operand
            };
        }

        return ParsePostfix();
    }

    private JsExpr ParsePostfix()
    {
        var expr = ParsePrimary();

        while (true)
        {
            if (CheckPunct("."))
            {
                Advance();
                var prop = ExpectIdent("expected property name");

                expr = new JsMember
                {
                    Obj = expr,
                    Prop = prop.Text
                };

                continue;
            }

            if (CheckPunct("["))
            {
                Advance();
                var index = ParseExpression();
                ExpectPunct("]", "expected ']'");

                expr = new JsIndex
                {
                    Obj = expr,
                    Index = index
                };

                continue;
            }

            if (CheckPunct("("))
            {
                Advance();
                var args = ParseArguments();
                ExpectPunct(")", "expected ')' after arguments");

                expr = new JsCall
                {
                    Callee = expr,
                    Args = args
                };

                continue;
            }

            break;
        }

        return expr;
    }

    private List<JsExpr> ParseArguments()
    {
        var args = new List<JsExpr>();

        if (CheckPunct(")"))
            return args;

        while (true)
        {
            args.Add(ParseExpression());

            if (!MatchPunct(","))
                break;
        }

        return args;
    }

    private JsExpr ParsePrimary()
    {
        if (Current.Type == JsTokenType.Number)
        {
            var t = Advance();
            return new JsNumber { Value = t.Number };
        }

        if (Current.Type == JsTokenType.String)
        {
            var t = Advance();
            return new JsString { Value = t.Text };
        }

        if (CheckKeyword("true"))
        {
            Advance();
            return new JsBool { Value = true };
        }

        if (CheckKeyword("false"))
        {
            Advance();
            return new JsBool { Value = false };
        }

        if (CheckKeyword("null"))
        {
            Advance();
            return new JsNull();
        }

        if (CheckKeyword("undefined"))
        {
            Advance();
            return new JsUndefinedExpr();
        }

        if (CheckKeyword("function"))
        {
            Advance();

            string name = null;

            if (Current.Type == JsTokenType.Ident)
                name = Advance().Text;

            ExpectPunct("(", "expected '(' in function expression");

            var pars = ParseParameters();

            ExpectPunct(")", "expected ')' in function expression");

            var body = ParseBlock();

            return new JsFunctionExpr
            {
                Name = name,
                Params = pars,
                Body = body
            };
        }

        if (Current.Type == JsTokenType.Ident)
        {
            var t = Advance();
            return new JsIdentifier { Name = t.Text };
        }

        if (CheckPunct("("))
        {
            Advance();
            var expr = ParseExpression();
            ExpectPunct(")", "expected ')'");
            return expr;
        }

        if (CheckPunct("["))
        {
            Advance();

            var arr = new JsArrayLit();

            if (!CheckPunct("]"))
            {
                while (true)
                {
                    arr.Items.Add(ParseExpression());

                    if (!MatchPunct(","))
                        break;
                }
            }

            ExpectPunct("]", "expected ']'");

            return arr;
        }

        if (CheckPunct("{"))
        {
            Advance();

            var obj = new JsObjectLit();

            if (!CheckPunct("}"))
            {
                while (true)
                {
                    string key;

                    if (Current.Type == JsTokenType.String)
                        key = Advance().Text;
                    else
                        key = ExpectIdent("expected object key").Text;

                    ExpectPunct(":", "expected ':' in object literal");

                    var value = ParseExpression();

                    obj.Props.Add((key, value));

                    if (!MatchPunct(","))
                        break;
                }
            }

            ExpectPunct("}", "expected '}'");

            return obj;
        }

        throw new Exception($"unexpected token '{Current.Text}'");
    }
}

public class JsReturnSignal : Exception
{
    public object Value;

    public JsReturnSignal(object value)
    {
        Value = value;
    }
}

public class JsBreakSignal : Exception
{
}

public class JsContinueSignal : Exception
{
}

public class JsDocument
{
    public JsInterpreter Interp;
}

public class JsDom
{
    public DomNode Node;
    public JsInterpreter Interp;
}

public class JsStyle
{
    public DomNode Node;
}

public class JsClassList
{
    public DomNode Node;
}

public class JsInterpreter
{
    public DomNode Root;
    public string ConsoleMessage = "";

    private readonly Scope global;

    public JsInterpreter()
    {
        global = new Scope(null);
        SetupGlobals();
    }

    private void SetupGlobals()
    {
        global.Define("undefined", JsUndefined.Value);
        global.Define("null", null);

        var consoleObj = new Dictionary<string, object>
        {
            ["log"] = new JsNative
            {
                Fn = args =>
                {
                    ConsoleMessage = string.Join(" ", args.Select(JsToString));
                    return null;
                }
            }
        };

        global.Define("console", consoleObj);

        global.Define("document", new JsDocument { Interp = this });

        global.Define("alert", new JsNative
        {
            Fn = args =>
            {
                ConsoleMessage = args.Count > 0 ? JsToString(args[0]) : "";
                return null;
            }
        });
    }

    public void Execute(string code)
    {
        if (string.IsNullOrWhiteSpace(code))
            return;

        var parser = new JsParser(code);
        var stmts = parser.ParseProgram();

        foreach (var stmt in stmts)
            Exec(stmt, global);
    }

    public object CallFunction(object callee, List<object> args)
    {
        if (callee is JsNative native)
            return native.Fn(args);

        if (callee is JsFunction func)
        {
            var scope = new Scope(func.Closure ?? global);

            for (int i = 0; i < func.Params.Count; i++)
                scope.Define(func.Params[i], i < args.Count ? args[i] : JsUndefined.Value);

            try
            {
                Exec(func.Body, scope);
            }
            catch (JsReturnSignal r)
            {
                return r.Value;
            }

            return JsUndefined.Value;
        }

        throw new Exception("attempt to call a non-function");
    }

    private void Exec(JsStmt stmt, Scope scope)
    {
        switch (stmt)
        {
            case JsBlock block:
                foreach (var s in block.Body)
                    Exec(s, scope);
                break;

            case JsVarDecl varDecl:
                scope.Define(varDecl.Name, varDecl.Init != null ? Eval(varDecl.Init, scope) : JsUndefined.Value);
                break;

            case JsExprStmt exprStmt:
                Eval(exprStmt.Expr, scope);
                break;

            case JsIf ifStmt:
                if (Truthy(Eval(ifStmt.Cond, scope)))
                    Exec(ifStmt.Then, scope);
                else if (ifStmt.Else != null)
                    Exec(ifStmt.Else, scope);
                break;

            case JsWhile whileStmt:
                while (Truthy(Eval(whileStmt.Cond, scope)))
                {
                    try
                    {
                        Exec(whileStmt.Body, scope);
                    }
                    catch (JsBreakSignal)
                    {
                        break;
                    }
                    catch (JsContinueSignal)
                    {
                    }
                }
                break;

            case JsFor forStmt:
            {
                var forScope = new Scope(scope);

                if (forStmt.Init != null)
                    Exec(forStmt.Init, forScope);

                while (forStmt.Cond == null || Truthy(Eval(forStmt.Cond, forScope)))
                {
                    try
                    {
                        Exec(forStmt.Body, forScope);
                    }
                    catch (JsBreakSignal)
                    {
                        break;
                    }
                    catch (JsContinueSignal)
                    {
                    }

                    if (forStmt.Update != null)
                        Eval(forStmt.Update, forScope);
                }

                break;
            }

            case JsReturn ret:
                throw new JsReturnSignal(ret.Expr != null ? Eval(ret.Expr, scope) : JsUndefined.Value);

            case JsBreak:
                throw new JsBreakSignal();

            case JsContinue:
                throw new JsContinueSignal();

            case JsFunctionDecl funcDecl:
                scope.Define(funcDecl.Name, new JsFunction
                {
                    Name = funcDecl.Name,
                    Params = funcDecl.Params,
                    Body = funcDecl.Body,
                    Closure = scope
                });
                break;

            default:
                throw new Exception("unknown JS statement");
        }
    }

    private object Eval(JsExpr expr, Scope scope)
    {
        switch (expr)
        {
            case JsNumber n:
                return n.Value;

            case JsString s:
                return s.Value;

            case JsBool b:
                return b.Value;

            case JsNull:
                return null;

            case JsUndefinedExpr:
                return JsUndefined.Value;

            case JsIdentifier ident:
                if (scope.TryGet(ident.Name, out object value))
                    return value;

                throw new Exception($"JS identifier not found: {ident.Name}");

            case JsAssign assign:
                return EvalAssign(assign, scope);

            case JsBinary binary:
                return EvalBinary(binary, scope);

            case JsLogical logical:
                return EvalLogical(logical, scope);

            case JsUnary unary:
                return EvalUnary(unary, scope);

            case JsCall call:
            {
                var callee = Eval(call.Callee, scope);
                var args = call.Args.Select(a => Eval(a, scope)).ToList();
                return CallFunction(callee, args);
            }

            case JsMember member:
            {
                var obj = Eval(member.Obj, scope);
                return GetMember(obj, member.Prop);
            }

            case JsIndex index:
            {
                var obj = Eval(index.Obj, scope);
                var idx = Eval(index.Index, scope);
                return GetIndex(obj, idx);
            }

            case JsObjectLit obj:
            {
                var dict = new Dictionary<string, object>(StringComparer.Ordinal);

                foreach (var prop in obj.Props)
                    dict[prop.Key] = Eval(prop.Value, scope);

                return dict;
            }

            case JsArrayLit arr:
            {
                var list = new List<object>();

                foreach (var item in arr.Items)
                    list.Add(Eval(item, scope));

                return list;
            }

            case JsFunctionExpr funcExpr:
                return new JsFunction
                {
                    Name = funcExpr.Name,
                    Params = funcExpr.Params,
                    Body = funcExpr.Body,
                    Closure = scope
                };

            default:
                throw new Exception("unknown JS expression");
        }
    }

    private object EvalAssign(JsAssign assign, Scope scope)
    {
        object value = Eval(assign.Value, scope);

        if (assign.Op == "+=" || assign.Op == "-=")
        {
            object current = Eval(assign.Target, scope);

            if (assign.Op == "+=")
                value = Add(current, value);
            else
                value = NumericOp("-", current, value);
        }

        switch (assign.Target)
        {
            case JsIdentifier ident:
                scope.Set(ident.Name, value);
                return value;

            case JsMember member:
            {
                var obj = Eval(member.Obj, scope);
                SetMember(obj, member.Prop, value);
                return value;
            }

            case JsIndex index:
            {
                var obj = Eval(index.Obj, scope);
                var idx = Eval(index.Index, scope);
                SetIndex(obj, idx, value);
                return value;
            }

            default:
                throw new Exception("invalid assignment target");
        }
    }

    private object EvalLogical(JsLogical logical, Scope scope)
    {
        var left = Eval(logical.Left, scope);

        if (logical.Op == "&&")
            return Truthy(left) ? Eval(logical.Right, scope) : left;

        if (logical.Op == "||")
            return Truthy(left) ? left : Eval(logical.Right, scope);

        throw new Exception("unknown logical operator");
    }

    private object EvalUnary(JsUnary unary, Scope scope)
    {
        var value = Eval(unary.Operand, scope);

        switch (unary.Op)
        {
            case "!":
                return !Truthy(value);

            case "-":
                return -ToNumber(value);

            case "+":
                return ToNumber(value);

            default:
                throw new Exception("unknown unary operator");
        }
    }

    private object EvalBinary(JsBinary binary, Scope scope)
    {
        var left = Eval(binary.Left, scope);
        var right = Eval(binary.Right, scope);

        switch (binary.Op)
        {
            case "+":
                return Add(left, right);

            case "-":
            case "*":
            case "/":
            case "%":
                return NumericOp(binary.Op, left, right);

            case "==":
            case "===":
                return LooseEquals(left, right);

            case "!=":
            case "!==":
                return !LooseEquals(left, right);

            case "<":
                return Compare(left, right) < 0;

            case "<=":
                return Compare(left, right) <= 0;

            case ">":
                return Compare(left, right) > 0;

            case ">=":
                return Compare(left, right) >= 0;

            default:
                throw new Exception($"unknown binary operator {binary.Op}");
        }
    }

    private object Add(object left, object right)
    {
        if (left is string || right is string)
            return JsToString(left) + JsToString(right);

        return ToNumber(left) + ToNumber(right);
    }

    private object NumericOp(string op, object left, object right)
    {
        double a = ToNumber(left);
        double b = ToNumber(right);

        return op switch
        {
            "-" => a - b,
            "*" => a * b,
            "/" => a / b,
            "%" => a % b,
            _ => throw new Exception($"unknown numeric operator {op}")
        };
    }

    private bool LooseEquals(object a, object b)
    {
        if (a == null && b == null)
            return true;

        if (a is JsUndefined && b is JsUndefined)
            return true;

        if ((a == null || a is JsUndefined) && (b == null || b is JsUndefined))
            return true;

        if (a is string sa && b is string sb)
            return sa == sb;

        if (a is bool ba && b is bool bb)
            return ba == bb;

        if (double.TryParse(JsToString(a), NumberStyles.Any, CultureInfo.InvariantCulture, out double da) &&
            double.TryParse(JsToString(b), NumberStyles.Any, CultureInfo.InvariantCulture, out double db))
        {
            return Math.Abs(da - db) < 1e-12;
        }

        return ReferenceEquals(a, b);
    }

    private int Compare(object left, object right)
    {
        if (left is string ls && right is string rs)
            return string.CompareOrdinal(ls, rs);

        return ToNumber(left).CompareTo(ToNumber(right));
    }

    public static bool Truthy(object value)
    {
        if (value == null || value is JsUndefined)
            return false;

        if (value is bool b)
            return b;

        if (value is double d)
            return d != 0;

        if (value is string s)
            return s.Length > 0;

        return true;
    }

    public static double ToNumber(object value)
    {
        if (value == null || value is JsUndefined)
            return 0;

        if (value is bool b)
            return b ? 1 : 0;

        if (value is double d)
            return d;

        if (double.TryParse(JsToString(value), NumberStyles.Any, CultureInfo.InvariantCulture, out double parsed))
            return parsed;

        return 0;
    }

    public static string JsToString(object value)
    {
        if (value == null)
            return "null";

        if (value is JsUndefined)
            return "undefined";

        if (value is bool b)
            return b ? "true" : "false";

        if (value is double d)
        {
            if (d == Math.Floor(d) && Math.Abs(d) < 1e15)
                return ((long)d).ToString(CultureInfo.InvariantCulture);

            return d.ToString("R", CultureInfo.InvariantCulture);
        }

        if (value is string s)
            return s;

        if (value is List<object> list)
            return string.Join(",", list.Select(JsToString));

        if (value is JsDom dom)
            return $"<{dom.Node.Tag}>";

        if (value is Dictionary<string, object>)
            return "[object Object]";

        return value.ToString();
    }

    private object GetIndex(object obj, object index)
    {
        if (obj is List<object> list)
        {
            int i = (int)ToNumber(index);
            return i >= 0 && i < list.Count ? list[i] : JsUndefined.Value;
        }

        if (obj is Dictionary<string, object> dict)
        {
            string key = JsToString(index);
            return dict.TryGetValue(key, out object value) ? value : JsUndefined.Value;
        }

        if (obj is string str)
        {
            int i = (int)ToNumber(index);
            return i >= 0 && i < str.Length ? str[i].ToString() : JsUndefined.Value;
        }

        return JsUndefined.Value;
    }

    private void SetIndex(object obj, object index, object value)
    {
        if (obj is List<object> list)
        {
            int i = (int)ToNumber(index);

            while (list.Count <= i)
                list.Add(JsUndefined.Value);

            list[i] = value;
            return;
        }

        if (obj is Dictionary<string, object> dict)
        {
            dict[JsToString(index)] = value;
            return;
        }

        throw new Exception("cannot set index on this value");
    }

    private object GetMember(object obj, string prop)
    {
        if (obj == null || obj is JsUndefined)
            throw new Exception($"cannot read property '{prop}' of null/undefined");

        if (obj is JsDocument doc)
            return GetDocumentMember(doc, prop);

        if (obj is JsDom dom)
            return GetDomMember(dom, prop);

        if (obj is JsStyle style)
        {
            string cssName = CamelToCss(prop);

            if (style.Node.InlineStyle.TryGetValue(cssName, out string inlineValue))
                return inlineValue;

            if (style.Node.ComputedStyle.TryGetValue(cssName, out string computedValue))
                return computedValue;

            return "";
        }

        if (obj is JsClassList classList)
            return GetClassListMember(classList, prop);

        if (obj is Dictionary<string, object> dict)
            return dict.TryGetValue(prop, out object dictValue) ? dictValue : JsUndefined.Value;

        if (obj is List<object> list)
        {
            if (prop == "length")
                return (double)list.Count;

            if (prop == "push")
            {
                return new JsNative
                {
                    Fn = args =>
                    {
                        foreach (var arg in args)
                            list.Add(arg);

                        return (double)list.Count;
                    }
                };
            }

            if (prop == "join")
            {
                return new JsNative
                {
                    Fn = args =>
                    {
                        string sep = args.Count > 0 ? JsToString(args[0]) : ",";
                        return string.Join(sep, list.Select(JsToString));
                    }
                };
            }
        }

        if (obj is string str)
        {
            if (prop == "length")
                return (double)str.Length;

            if (prop == "toUpperCase")
                return new JsNative { Fn = _ => str.ToUpperInvariant() };

            if (prop == "toLowerCase")
                return new JsNative { Fn = _ => str.ToLowerInvariant() };

            if (prop == "includes")
            {
                return new JsNative
                {
                    Fn = args => str.Contains(args.Count > 0 ? JsToString(args[0]) : "", StringComparison.Ordinal)
                };
            }
        }

        return JsUndefined.Value;
    }

    private object GetDocumentMember(JsDocument doc, string prop)
    {
        switch (prop)
        {
            case "body":
                return new JsDom
                {
                    Node = FindBody(doc.Interp.Root),
                    Interp = doc.Interp
                };

            case "title":
                return FindTitle(doc.Interp.Root);

            case "getElementById":
                return new JsNative
                {
                    Fn = args =>
                    {
                        if (args.Count == 0)
                            return null;

                        string id = JsToString(args[0]);
                        var node = StyleEngine.FindById(doc.Interp.Root, id);

                        if (node == null)
                            return null;

                        return new JsDom
                        {
                            Node = node,
                            Interp = doc.Interp
                        };
                    }
                };

            case "querySelector":
                return new JsNative
                {
                    Fn = args =>
                    {
                        if (args.Count == 0)
                            return null;

                        string selector = JsToString(args[0]);
                        var node = StyleEngine.FirstMatch(doc.Interp.Root, selector);

                        if (node == null)
                            return null;

                        return new JsDom
                        {
                            Node = node,
                            Interp = doc.Interp
                        };
                    }
                };

            case "querySelectorAll":
                return new JsNative
                {
                    Fn = args =>
                    {
                        var list = new List<object>();

                        if (args.Count == 0)
                            return list;

                        string selector = JsToString(args[0]);
                        var nodes = StyleEngine.AllMatches(doc.Interp.Root, selector);

                        foreach (var node in nodes)
                        {
                            list.Add(new JsDom
                            {
                                Node = node,
                                Interp = doc.Interp
                            });
                        }

                        return list;
                    }
                };

            case "createElement":
                return new JsNative
                {
                    Fn = args =>
                    {
                        string tag = args.Count > 0 ? JsToString(args[0]) : "div";

                        var node = new DomNode
                        {
                            Tag = tag.ToLowerInvariant()
                        };

                        return new JsDom
                        {
                            Node = node,
                            Interp = doc.Interp
                        };
                    }
                };

            case "write":
                return new JsNative
                {
                    Fn = args =>
                    {
                        string text = string.Join("", args.Select(JsToString));
                        var body = FindBody(doc.Interp.Root);

                        body.Children.Add(new DomNode
                        {
                            Tag = "#text",
                            Text = text,
                            Parent = body
                        });

                        return null;
                    }
                };

            default:
                return JsUndefined.Value;
        }
    }

    private object GetDomMember(JsDom dom, string prop)
    {
        switch (prop)
        {
            case "id":
                return dom.Node.Attrs.TryGetValue("id", out string id) ? id : "";

            case "tagName":
                return dom.Node.Tag.ToUpperInvariant();

            case "innerHTML":
                return GetInnerHTML(dom.Node);

            case "textContent":
                return GetTextContent(dom.Node);

            case "style":
                return new JsStyle { Node = dom.Node };

            case "classList":
                return new JsClassList { Node = dom.Node };

            case "setAttribute":
                return new JsNative
                {
                    Fn = args =>
                    {
                        if (args.Count >= 2)
                            dom.Node.Attrs[JsToString(args[0])] = JsToString(args[1]);

                        return null;
                    }
                };

            case "getAttribute":
                return new JsNative
                {
                    Fn = args =>
                    {
                        if (args.Count == 0)
                            return null;

                        string name = JsToString(args[0]);

                        return dom.Node.Attrs.TryGetValue(name, out string value) ? value : null;
                    }
                };

            case "addEventListener":
                return new JsNative
                {
                    Fn = args =>
                    {
                        if (args.Count >= 2 && JsToString(args[0]).ToLowerInvariant() == "click")
                            dom.Node.ClickListeners.Add(args[1]);

                        return null;
                    }
                };

            case "appendChild":
                return new JsNative
                {
                    Fn = args =>
                    {
                        if (args.Count >= 1 && args[0] is JsDom child)
                        {
                            child.Node.Parent = dom.Node;
                            dom.Node.Children.Add(child.Node);
                        }

                        return null;
                    }
                };

            case "remove":
                return new JsNative
                {
                    Fn = _ =>
                    {
                        if (dom.Node.Parent != null)
                            dom.Node.Parent.Children.Remove(dom.Node);

                        return null;
                    }
                };

            case "onclick":
                return JsUndefined.Value;

            default:
                return dom.Node.Attrs.TryGetValue(prop, out string attr) ? attr : JsUndefined.Value;
        }
    }

    private object GetClassListMember(JsClassList classList, string prop)
    {
        switch (prop)
        {
            case "add":
                return new JsNative
                {
                    Fn = args =>
                    {
                        foreach (var arg in args)
                            AddClass(classList.Node, JsToString(arg));

                        return null;
                    }
                };

            case "remove":
                return new JsNative
                {
                    Fn = args =>
                    {
                        foreach (var arg in args)
                            RemoveClass(classList.Node, JsToString(arg));

                        return null;
                    }
                };

            case "toggle":
                return new JsNative
                {
                    Fn = args =>
                    {
                        if (args.Count == 0)
                            return false;

                        string cls = JsToString(args[0]);

                        if (HasClass(classList.Node, cls))
                        {
                            RemoveClass(classList.Node, cls);
                            return false;
                        }

                        AddClass(classList.Node, cls);
                        return true;
                    }
                };

            case "contains":
                return new JsNative
                {
                    Fn = args =>
                    {
                        if (args.Count == 0)
                            return false;

                        return HasClass(classList.Node, JsToString(args[0]));
                    }
                };

            default:
                return JsUndefined.Value;
        }
    }

    private void SetMember(object obj, string prop, object value)
    {
        if (obj is JsDocument doc)
        {
            if (prop == "title")
            {
                // title set is accepted but not rendered specially
                return;
            }

            throw new Exception($"cannot set document.{prop}");
        }

        if (obj is JsDom dom)
        {
            SetDomMember(dom, prop, value);
            return;
        }

        if (obj is JsStyle style)
        {
            string cssName = CamelToCss(prop);
            style.Node.InlineStyle[cssName] = JsToString(value);
            return;
        }

        if (obj is Dictionary<string, object> dict)
        {
            dict[prop] = value;
            return;
        }

        if (obj is List<object> list)
        {
            if (prop == "length")
            {
                int len = (int)ToNumber(value);

                while (list.Count < len)
                    list.Add(JsUndefined.Value);

                while (list.Count > len)
                    list.RemoveAt(list.Count - 1);

                return;
            }

            throw new Exception($"cannot set list property {prop}");
        }

        throw new Exception("cannot set member on this value");
    }

    private void SetDomMember(JsDom dom, string prop, object value)
    {
        switch (prop)
        {
            case "id":
                dom.Node.Attrs["id"] = JsToString(value);
                return;

            case "className":
                dom.Node.Attrs["class"] = JsToString(value);
                return;

            case "innerHTML":
                SetInnerHTML(dom.Node, JsToString(value));
                return;

            case "textContent":
                SetTextContent(dom.Node, JsToString(value));
                return;

            case "onclick":
                if (value is JsFunction func)
                {
                    dom.Node.ClickListeners.Add(func);
                }
                else if (value is string source)
                {
                    dom.Node.OnClickSource = source;
                }
                else if (value == null || value is JsUndefined)
                {
                    dom.Node.OnClickSource = null;
                }
                return;

            default:
                dom.Node.Attrs[prop] = JsToString(value);
                return;
        }
    }

    private static string CamelToCss(string name)
    {
        var sb = new StringBuilder();

        foreach (char c in name)
        {
            if (char.IsUpper(c))
            {
                sb.Append('-');
                sb.Append(char.ToLowerInvariant(c));
            }
            else
            {
                sb.Append(c);
            }
        }

        return sb.ToString();
    }

    private static void AddClass(DomNode node, string cls)
    {
        string current = node.Attrs.TryGetValue("class", out string c) ? c : "";
        var classes = current.Split(' ', StringSplitOptions.RemoveEmptyEntries).ToList();

        if (!classes.Contains(cls, StringComparer.OrdinalIgnoreCase))
            classes.Add(cls);

        node.Attrs["class"] = string.Join(" ", classes);
    }

    private static void RemoveClass(DomNode node, string cls)
    {
        string current = node.Attrs.TryGetValue("class", out string c) ? c : "";
        var classes = current.Split(' ', StringSplitOptions.RemoveEmptyEntries)
            .Where(x => !x.Equals(cls, StringComparison.OrdinalIgnoreCase))
            .ToList();

        node.Attrs["class"] = string.Join(" ", classes);
    }

    private static bool HasClass(DomNode node, string cls)
    {
        string current = node.Attrs.TryGetValue("class", out string c) ? c : "";
        return current.Split(' ', StringSplitOptions.RemoveEmptyEntries)
            .Any(x => x.Equals(cls, StringComparison.OrdinalIgnoreCase));
    }

    private static DomNode FindBody(DomNode root)
    {
        var body = StyleEngine.FirstMatch(root, "body");
        return body ?? root;
    }

    private static string FindTitle(DomNode root)
    {
        var title = StyleEngine.FirstMatch(root, "title");
        return title != null ? GetTextContent(title) : "";
    }

    private static string GetTextContent(DomNode node)
    {
        if (node.IsText)
            return node.Text;

        var sb = new StringBuilder();

        foreach (var child in node.Children)
            sb.Append(GetTextContent(child));

        return sb.ToString();
    }

    private static void SetTextContent(DomNode node, string text)
    {
        node.Children.Clear();

        node.Children.Add(new DomNode
        {
            Tag = "#text",
            Text = text,
            Parent = node
        });
    }

    private static string GetInnerHTML(DomNode node)
    {
        var sb = new StringBuilder();

        foreach (var child in node.Children)
            AppendHtml(child, sb);

        return sb.ToString();
    }

    private static void SetInnerHTML(DomNode node, string html)
    {
        var fragment = HtmlParser.Parse(html);

        node.Children.Clear();

        foreach (var child in fragment.Children)
        {
            child.Parent = node;
            node.Children.Add(child);
        }
    }

    private static void AppendHtml(DomNode node, StringBuilder sb)
    {
        if (node.IsText)
        {
            sb.Append(node.Text);
            return;
        }

        sb.Append('<');
        sb.Append(node.Tag);

        foreach (var attr in node.Attrs)
            sb.Append($" {attr.Key}=\"{attr.Value}\"");

        sb.Append('>');

        foreach (var child in node.Children)
            AppendHtml(child, sb);

        sb.Append("</");
        sb.Append(node.Tag);
        sb.Append('>');
    }
}

public class PageEngine
{
    public string Url = "";
    public DomNode Root;
    public List<CssRule> Styles = new();
    public JsInterpreter Js;

    public static PageEngine Load(string url, string html)
    {
        var page = new PageEngine
        {
            Url = url,
            Root = HtmlParser.Parse(html)
        };

        CollectStyles(page.Root, page.Styles);

        page.Js = new JsInterpreter
        {
            Root = page.Root
        };

        page.RefreshStyles();
        ExecuteScripts(page.Root, page);
        page.RefreshStyles();

        return page;
    }

    public void RefreshStyles()
    {
        StyleEngine.ComputeAll(Root, Styles);
    }

    public void ClickNode(DomNode node)
    {
        if (node == null)
            return;

        if (!string.IsNullOrWhiteSpace(node.OnClickSource))
        {
            try
            {
                Js.Execute(node.OnClickSource);
            }
            catch (Exception e)
            {
                Js.ConsoleMessage = $"JS error: {e.Message}";
            }
        }

        foreach (var listener in node.ClickListeners.ToList())
        {
            try
            {
                if (listener is JsFunction func)
                    Js.CallFunction(func, new List<object>());
                else if (listener is string source)
                    Js.Execute(source);
            }
            catch (Exception e)
            {
                Js.ConsoleMessage = $"JS error: {e.Message}";
            }
        }

        RefreshStyles();
    }

    public DomNode FindNodeById(int id)
    {
        return FindNodeById(Root, id);
    }

    private DomNode FindNodeById(DomNode node, int id)
    {
        if (node == null)
            return null;

        if (node.Id == id)
            return node;

        foreach (var child in node.Children)
        {
            var found = FindNodeById(child, id);

            if (found != null)
                return found;
        }

        return null;
    }

    private static void CollectStyles(DomNode node, List<CssRule> styles)
    {
        if (node == null)
            return;

        if (node.Tag.Equals("style", StringComparison.OrdinalIgnoreCase))
            styles.AddRange(CssParser.Parse(node.Text));

        foreach (var child in node.Children)
            CollectStyles(child, styles);
    }

    private static void ExecuteScripts(DomNode node, PageEngine page)
    {
        if (node == null)
            return;

        if (node.Tag.Equals("script", StringComparison.OrdinalIgnoreCase))
        {
            if (!string.IsNullOrWhiteSpace(node.Text))
            {
                try
                {
                    page.Js.Execute(node.Text);
                }
                catch (Exception e)
                {
                    page.Js.ConsoleMessage = $"JS error: {e.Message}";
                }
            }
        }

        foreach (var child in node.Children.ToList())
            ExecuteScripts(child, page);
    }
}

public class RenderBuffer
{
    public int Width;
    public int X;

    public List<List<char>> Chars = new();
    public List<List<ConsoleColor>> Fg = new();
    public List<List<ConsoleColor>> Bg = new();
    public List<List<int>> Elem = new();

    public RenderBuffer(int width)
    {
        Width = Math.Max(20, width);
        NewLine();
    }

    public int Height => Chars.Count;

    public void NewLine()
    {
        Chars.Add(new List<char>());
        Fg.Add(new List<ConsoleColor>());
        Bg.Add(new List<ConsoleColor>());
        Elem.Add(new List<int>());
        X = 0;
    }

    public void Write(char c, ConsoleColor fg, ConsoleColor bg, int elem)
    {
        if (X >= Width)
            NewLine();

        Chars[^1].Add(c);
        Fg[^1].Add(fg);
        Bg[^1].Add(bg);
        Elem[^1].Add(elem);
        X++;
    }

    public char LastChar()
    {
        if (Chars.Count == 0)
            return '\0';

        var row = Chars[^1];

        return row.Count == 0 ? '\0' : row[^1];
    }
}

public static class AsciiRenderer
{
    public static RenderBuffer Render(PageEngine page, int width)
    {
        var buffer = new RenderBuffer(width);

        if (page?.Root != null)
            RenderNode(page.Root, buffer, -1);

        while (buffer.Height > 1 && buffer.Chars[^1].Count == 0)
        {
            buffer.Chars.RemoveAt(buffer.Chars.Count - 1);
            buffer.Fg.RemoveAt(buffer.Fg.Count - 1);
            buffer.Bg.RemoveAt(buffer.Bg.Count - 1);
            buffer.Elem.RemoveAt(buffer.Elem.Count - 1);
        }

        return buffer;
    }

    private static void RenderNode(DomNode node, RenderBuffer buffer, int interactiveId)
    {
        if (node == null)
            return;

        if (node.IsText)
        {
            var style = node.Parent?.ComputedStyle ?? node.ComputedStyle;
            RenderText(node.Text, style, buffer, interactiveId);
            return;
        }

        var computed = node.ComputedStyle;

        if (computed.TryGetValue("display", out string display) && display == "none")
            return;

        string tag = node.Tag.ToLowerInvariant();

        if (tag == "br")
        {
            buffer.NewLine();
            return;
        }

        if (tag == "hr")
        {
            if (buffer.X > 0)
                buffer.NewLine();

            var fg = StyleEngine.ParseColor(GetStyleValue(computed, "color"), ConsoleColor.Gray);
            var bg = StyleEngine.ParseColor(GetStyleValue(computed, "background-color"), ConsoleColor.Black);

            for (int i = 0; i < buffer.Width; i++)
                buffer.Write('-', fg, bg, interactiveId);

            buffer.NewLine();
            return;
        }

        bool isBlock = computed.TryGetValue("display", out string d) && d == "block";

        if (isBlock && buffer.X > 0)
            buffer.NewLine();

        int indent =
            StyleEngine.GetInt(computed, "margin-left") +
            StyleEngine.GetInt(computed, "padding-left") +
            StyleEngine.GetInt(computed, "text-indent");

        if (tag == "li")
            indent = Math.Max(indent, 2);

        if (isBlock && indent > 0)
        {
            var indentFg = StyleEngine.ParseColor(GetStyleValue(computed, "color"), ConsoleColor.Gray);
            var indentBg = StyleEngine.ParseColor(GetStyleValue(computed, "background-color"), ConsoleColor.Black);

            for (int i = 0; i < indent && buffer.X < buffer.Width; i++)
                buffer.Write(' ', indentFg, indentBg, interactiveId);
        }

        int newInteractive = IsInteractive(node) ? node.Id : interactiveId;

        if (tag == "li" && isBlock)
        {
            var liFg = StyleEngine.ParseColor(GetStyleValue(computed, "color"), ConsoleColor.Gray);
            var liBg = StyleEngine.ParseColor(GetStyleValue(computed, "background-color"), ConsoleColor.Black);

            buffer.Write('-', liFg, liBg, newInteractive);
            buffer.Write(' ', liFg, liBg, newInteractive);
        }

        if (tag == "button")
        {
            var btnFg = StyleEngine.ParseColor(GetStyleValue(computed, "color"), ConsoleColor.Green);
            var btnBg = StyleEngine.ParseColor(GetStyleValue(computed, "background-color"), ConsoleColor.Black);

            buffer.Write('[', btnFg, btnBg, newInteractive);

            foreach (var child in node.Children)
                RenderNode(child, buffer, newInteractive);

            buffer.Write(']', btnFg, btnBg, newInteractive);

            if (isBlock && buffer.X > 0)
                buffer.NewLine();

            return;
        }

        if (tag == "img")
        {
            var fg = StyleEngine.ParseColor(GetStyleValue(computed, "color"), ConsoleColor.Gray);
            var bg = StyleEngine.ParseColor(GetStyleValue(computed, "background-color"), ConsoleColor.Black);

            string alt = node.Attrs.TryGetValue("alt", out string a) ? a : "img";

            foreach (char c in $"[{alt}]")
                buffer.Write(c, fg, bg, newInteractive);

            if (isBlock && buffer.X > 0)
                buffer.NewLine();

            return;
        }

        if (tag == "input")
        {
            var fg = StyleEngine.ParseColor(GetStyleValue(computed, "color"), ConsoleColor.Gray);
            var bg = StyleEngine.ParseColor(GetStyleValue(computed, "background-color"), ConsoleColor.Black);

            string value = node.Attrs.TryGetValue("value", out string v) ? v : "";

            foreach (char c in $"[{value}]")
                buffer.Write(c, fg, bg, newInteractive);

            if (isBlock && buffer.X > 0)
                buffer.NewLine();

            return;
        }

        foreach (var child in node.Children)
            RenderNode(child, buffer, newInteractive);

        if (isBlock && buffer.X > 0)
            buffer.NewLine();
    }

    private static void RenderText(string text, Dictionary<string, string> style, RenderBuffer buffer, int interactiveId)
    {
        if (string.IsNullOrWhiteSpace(text))
            return;

        var fg = StyleEngine.ParseColor(GetStyleValue(style, "color"), ConsoleColor.Gray);
        var bg = StyleEngine.ParseColor(GetStyleValue(style, "background-color"), ConsoleColor.Black);

        string normalized = NormalizeWhitespace(text);

        foreach (char raw in normalized)
        {
            char c = raw;

            if (c < 32 || c > 126)
                c = '?';

            if (c == ' ')
            {
                if (buffer.X == 0 || buffer.LastChar() == ' ')
                    continue;

                buffer.Write(' ', fg, bg, interactiveId);
                continue;
            }

            buffer.Write(c, fg, bg, interactiveId);
        }
    }

    private static string NormalizeWhitespace(string text)
    {
        return text
            .Replace('\r', ' ')
            .Replace('\n', ' ')
            .Replace('\t', ' ');
    }

    private static bool IsInteractive(DomNode node)
    {
        if (node == null || node.IsText)
            return false;

        if (node.Tag.Equals("a", StringComparison.OrdinalIgnoreCase))
            return true;

        if (node.Tag.Equals("button", StringComparison.OrdinalIgnoreCase))
            return true;

        if (!string.IsNullOrWhiteSpace(node.OnClickSource))
            return true;

        if (node.ClickListeners.Count > 0)
            return true;

        return false;
    }

    private static string GetStyleValue(Dictionary<string, string> style, string name)
    {
        if (style == null)
            return "";

        return style.TryGetValue(name, out string value) ? value : "";
    }
}

public class AsciiBrowser
{
    private const int STD_INPUT_HANDLE = -10;

    private const uint ENABLE_MOUSE_INPUT = 0x0010;
    private const uint ENABLE_QUICK_EDIT = 0x0040;
    private const uint ENABLE_EXTENDED_FLAGS = 0x0080;

    private const ushort KEY_EVENT = 0x0001;
    private const ushort MOUSE_EVENT = 0x0002;
    private const ushort WINDOW_BUFFER_SIZE_EVENT = 0x0004;

    private const uint LEFT_CTRL_PRESSED = 0x0008;
    private const uint RIGHT_CTRL_PRESSED = 0x0004;
    private const uint FROM_LEFT_1ST_BUTTON_PRESSED = 0x0001;

    private IntPtr inputHandle;
    private uint oldMode;
    private bool inputSetup;

    private bool running = true;

    private PageEngine page;
    private RenderBuffer rendered;

    private readonly List<string> history = new();
    private int historyIndex = -1;

    private int scrollY;
    private int mouseX;
    private int mouseY;
    private int hoverElem;

    public void Run(string target)
    {
        if (Console.IsInputRedirected || Console.IsOutputRedirected)
        {
            Console.Error.WriteLine("browse: interactive console is required.");
            return;
        }

        if (!SetupInput())
        {
            Console.Error.WriteLine("browse: failed to initialize console input.");
            return;
        }

        try
        {
            Console.Clear();
            Navigate(target, true);

            while (running)
            {
                Render();
                WaitForEvent();
            }
        }
        finally
        {
            RestoreInput();

            try
            {
                Console.Clear();
            }
            catch
            {
            }

            Console.WriteLine("Jash browser exited.");
        }
    }

    private bool SetupInput()
    {
        try
        {
            inputHandle = GetStdHandle(STD_INPUT_HANDLE);

            if (inputHandle == IntPtr.Zero || inputHandle == new IntPtr(-1))
                return false;

            if (!GetConsoleMode(inputHandle, out oldMode))
                return false;

            uint mode = oldMode;
            mode |= ENABLE_MOUSE_INPUT;
            mode |= ENABLE_EXTENDED_FLAGS;
            mode &= ~ENABLE_QUICK_EDIT;

            if (!SetConsoleMode(inputHandle, mode))
                return false;

            inputSetup = true;

            try
            {
                Console.CursorVisible = false;
                Console.TreatControlCAsInput = true;
            }
            catch
            {
            }

            return true;
        }
        catch
        {
            return false;
        }
    }

    private void RestoreInput()
    {
        try
        {
            if (inputSetup)
                SetConsoleMode(inputHandle, oldMode);

            Console.CursorVisible = true;
            Console.TreatControlCAsInput = false;
            Console.ResetColor();
        }
        catch
        {
        }
    }

    private void Navigate(string url, bool pushHistory)
    {
        try
        {
            if (pushHistory)
            {
                if (historyIndex + 1 < history.Count)
                    history.RemoveRange(historyIndex + 1, history.Count - historyIndex - 1);

                history.Add(url);
                historyIndex = history.Count - 1;
            }

            string html = Web.FetchString(url);

            page = PageEngine.Load(url, html);

            scrollY = 0;
            hoverElem = -1;

            RebuildRendered();
        }
        catch (Exception e)
        {
            string errorHtml =
                "<html><body>" +
                "<h1>Jash Browser Error</h1>" +
                "<p>" + (e.Message ?? "").Replace("<", "&lt;") + "</p>" +
                "<p>" + (url ?? "").Replace("<", "&lt;") + "</p>" +
                "</body></html>";

            page = PageEngine.Load(url, errorHtml);

            scrollY = 0;
            hoverElem = -1;

            RebuildRendered();
        }
    }

    private void RebuildRendered()
    {
        int width = Math.Max(20, Console.WindowWidth - 1);
        rendered = AsciiRenderer.Render(page, width);
        ClampScroll();
    }

    private void Back()
    {
        if (historyIndex > 0)
        {
            historyIndex--;
            Navigate(history[historyIndex], false);
        }
    }

    private void Forward()
    {
        if (historyIndex + 1 < history.Count)
        {
            historyIndex++;
            Navigate(history[historyIndex], false);
        }
    }

    private void Reload()
    {
        if (history.Count > 0)
            Navigate(history[historyIndex], false);
    }

    private void ClampScroll()
    {
        if (rendered == null)
        {
            scrollY = 0;
            return;
        }

        int visible = Math.Max(1, Console.WindowHeight - 2);
        int max = Math.Max(0, rendered.Height - visible);

        if (scrollY < 0)
            scrollY = 0;

        if (scrollY > max)
            scrollY = max;
    }

    private void Render()
    {
        try
        {
            ClampScroll();

            int width = Math.Max(20, Console.WindowWidth - 1);
            int height = Math.Max(1, Console.WindowHeight);
            int visible = height - 2;

            Console.ResetColor();
            Console.SetCursorPosition(0, 0);

            Console.BackgroundColor = ConsoleColor.DarkGray;
            Console.ForegroundColor = ConsoleColor.Black;

            string status =
                $" {Fit(page?.Url ?? "about:blank", Math.Max(10, width - 50))} | hover:{hoverElem} | {Fit(page?.Js?.ConsoleMessage ?? "", 30)} | Ctrl+X exit ";

            Console.Write(Fit(status, width));

            Console.ResetColor();

            if (rendered == null)
                return;

            for (int row = 0; row < visible; row++)
            {
                int y = scrollY + row;

                Console.SetCursorPosition(0, row + 1);

                if (y >= rendered.Height)
                {
                    Console.Write(new string(' ', width));
                    continue;
                }

                var chars = rendered.Chars[y];
                var fg = rendered.Fg[y];
                var bg = rendered.Bg[y];
                var elem = rendered.Elem[y];

                for (int x = 0; x < width; x++)
                {
                    char c = x < chars.Count ? chars[x] : ' ';
                    ConsoleColor f = x < fg.Count ? fg[x] : ConsoleColor.Gray;
                    ConsoleColor b = x < bg.Count ? bg[x] : ConsoleColor.Black;
                    int e = x < elem.Count ? elem[x] : -1;

                    if (e > 0 && e == hoverElem)
                    {
                        Console.BackgroundColor = ConsoleColor.Cyan;
                        Console.ForegroundColor = ConsoleColor.Black;
                    }
                    else
                    {
                        Console.BackgroundColor = b;
                        Console.ForegroundColor = f;
                    }

                    Console.Write(c);
                }
            }

            if (mouseY >= 1 && mouseY < visible + 1 && mouseX >= 0 && mouseX < width)
            {
                int pageY = scrollY + mouseY - 1;

                char c = ' ';

                if (pageY >= 0 && pageY < rendered.Height && mouseX < rendered.Chars[pageY].Count)
                    c = rendered.Chars[pageY][mouseX];

                Console.SetCursorPosition(mouseX, mouseY);
                Console.BackgroundColor = ConsoleColor.White;
                Console.ForegroundColor = ConsoleColor.Black;
                Console.Write(c == '\0' ? ' ' : c);
            }

            Console.SetCursorPosition(0, 0);
        }
        catch
        {
        }
    }

    private void WaitForEvent()
    {
        var records = new INPUT_RECORD[1];

        if (!ReadConsoleInput(inputHandle, records, 1, out uint read) || read == 0)
        {
            Thread.Sleep(20);
            return;
        }

        var rec = records[0];

        if (rec.EventType == KEY_EVENT)
            HandleKey(rec.KeyEvent);
        else if (rec.EventType == MOUSE_EVENT)
            HandleMouse(rec.MouseEvent);
        else if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT)
            RebuildRendered();
    }

    private void HandleKey(KEY_EVENT_RECORD e)
    {
        if (e.KeyDown == 0)
            return;

        bool ctrl = (e.ControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;

        if (ctrl && (e.VirtualKeyCode == 0x58 || e.UnicodeChar == 'x' || e.UnicodeChar == 'X'))
        {
            running = false;
            return;
        }

        switch (e.VirtualKeyCode)
        {
            case 0x26: // up
                scrollY--;
                break;

            case 0x28: // down
                scrollY++;
                break;

            case 0x21: // page up
                scrollY -= Math.Max(1, Console.WindowHeight - 2);
                break;

            case 0x22: // page down
                scrollY += Math.Max(1, Console.WindowHeight - 2);
                break;

            case 0x24: // home
                scrollY = 0;
                break;

            case 0x23: // end
                scrollY = int.MaxValue;
                break;

            case 0x0D: // enter
                if (hoverElem > 0)
                    ClickElement(hoverElem);
                break;

            default:
                char c = e.UnicodeChar;

                if (c == 'b' || c == 'B')
                    Back();
                else if (c == 'f' || c == 'F')
                    Forward();
                else if (c == 'r' || c == 'R')
                    Reload();
                break;
        }

        ClampScroll();
    }

    private void HandleMouse(MOUSE_EVENT_RECORD e)
    {
        mouseX = e.X;
        mouseY = e.Y;

        int visibleTop = 1;

        if (mouseY < visibleTop)
        {
            hoverElem = -1;
            return;
        }

        int pageY = scrollY + mouseY - visibleTop;
        int pageX = mouseX;

        hoverElem = GetElemAt(pageX, pageY);

        if ((e.ButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) != 0 && hoverElem > 0)
        {
            ClickElement(hoverElem);
        }
    }

    private int GetElemAt(int x, int y)
    {
        if (rendered == null)
            return -1;

        if (y < 0 || y >= rendered.Height)
            return -1;

        var row = rendered.Elem[y];

        if (x < 0 || x >= row.Count)
            return -1;

        return row[x];
    }

    private void ClickElement(int elemId)
    {
        if (page == null)
            return;

        var node = page.FindNodeById(elemId);

        if (node == null)
            return;

        if (node.Tag.Equals("a", StringComparison.OrdinalIgnoreCase))
        {
            if (node.Attrs.TryGetValue("href", out string href))
            {
                string resolved = Web.ResolveUrl(page.Url, href);
                Navigate(resolved, true);
                return;
            }
        }

        page.ClickNode(node);
        RebuildRendered();
    }

    private static string Fit(string s, int len)
    {
        if (len <= 0)
            return "";

        if (string.IsNullOrEmpty(s))
            return new string(' ', len);

        if (s.Length > len)
            return s.Substring(0, len);

        return s.PadRight(len);
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr GetStdHandle(int nStdHandle);

    [DllImport("kernel32.dll")]
    private static extern bool GetConsoleMode(IntPtr hConsoleHandle, out uint lpMode);

    [DllImport("kernel32.dll")]
    private static extern bool SetConsoleMode(IntPtr hConsoleHandle, uint dwMode);

    [DllImport("kernel32.dll", EntryPoint = "ReadConsoleInputW", CharSet = CharSet.Unicode)]
    private static extern bool ReadConsoleInput(
        IntPtr hConsoleInput,
        [Out] INPUT_RECORD[] lpBuffer,
        uint nLength,
        out uint lpNumberOfEventsRead);

    [StructLayout(LayoutKind.Explicit, Size = 20)]
    private struct INPUT_RECORD
    {
        [FieldOffset(0)]
        public ushort EventType;

        [FieldOffset(4)]
        public KEY_EVENT_RECORD KeyEvent;

        [FieldOffset(4)]
        public MOUSE_EVENT_RECORD MouseEvent;

        [FieldOffset(4)]
        public WINDOW_BUFFER_SIZE_RECORD WindowEvent;
    }

    [StructLayout(LayoutKind.Explicit, Size = 16)]
    private struct KEY_EVENT_RECORD
    {
        [FieldOffset(0)]
        public int KeyDown;

        [FieldOffset(4)]
        public ushort RepeatCount;

        [FieldOffset(6)]
        public ushort VirtualKeyCode;

        [FieldOffset(8)]
        public ushort VirtualScanCode;

        [FieldOffset(10)]
        public char UnicodeChar;

        [FieldOffset(12)]
        public uint ControlKeyState;
    }

    [StructLayout(LayoutKind.Explicit, Size = 16)]
    private struct MOUSE_EVENT_RECORD
    {
        [FieldOffset(0)]
        public short X;

        [FieldOffset(2)]
        public short Y;

        [FieldOffset(4)]
        public uint ButtonState;

        [FieldOffset(8)]
        public uint ControlKeyState;

        [FieldOffset(12)]
        public uint EventFlags;
    }

    [StructLayout(LayoutKind.Explicit, Size = 4)]
    private struct WINDOW_BUFFER_SIZE_RECORD
    {
        [FieldOffset(0)]
        public short X;

        [FieldOffset(2)]
        public short Y;
    }
}
```
