# I finally made the command, but booting is still difficult, so I’ll add it later. I’ll just release the code first
> **Code**
### Project Structure
```Jash/
 ├─ Jash.csproj
 └─ Program.cs
```
### Jash.csproj
``` <Project Sdk="Microsoft.NET.Sdk">
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
    <Using Include="System.Collections.Concurrent" />
    <Using Include="System.Globalization" />
  </ItemGroup>
</Project>
```
### Program.cs
```cs
 global using System.Diagnostics;
global using System.Text;
global using System.Collections.Concurrent;
global using System.Globalization;

namespace Jash;

public enum TokenType
{
    EOF,
    Newline,
    Identifier,
    Keyword,
    Word,
    Number,
    String,

    Assign,
    Eq,
    NotEq,
    Lt,
    LtEq,
    Gt,
    GtEq,
    GtGt,

    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    DotDot,

    LParen,
    RParen,
    Comma,
    Semicolon,

    Pipe,
    AmpAmp,
    OrOr
}

public class Token
{
    public TokenType Type;
    public string Text;
    public int Line;
    public int Column;

    public Token(TokenType type, string text, int line, int column)
    {
        Type = type;
        Text = text;
        Line = line;
        Column = column;
    }
}

public class JashError : Exception
{
    public string File;
    public int Line;
    public int Column;
    public bool Incomplete;
    public string Suggestion;

    public JashError(string message, Token token = null) : base(message)
    {
        if (token != null)
        {
            Line = token.Line;
            Column = token.Column;
        }
        else
        {
            Line = 1;
            Column = 1;
        }
    }

    public string Format()
    {
        var location = string.IsNullOrEmpty(File) ? "<input>" : File;
        var msg = $"Jash Error:\n{location}:{Line}:{Column}\n{Message}";
        if (!string.IsNullOrEmpty(Suggestion))
            msg += $"\n\nDid you mean '{Suggestion}'?";
        return msg;
    }
}

public class Lexer
{
    private static readonly HashSet<string> Keywords = new()
    {
        "if", "then", "else", "elseif", "end",
        "while", "do", "for", "in",
        "function", "return", "break",
        "and", "or", "not",
        "true", "false", "nil"
    };

    private readonly string src;
    private readonly string file;
    private int pos;
    private int line = 1;
    private int col = 1;
    private Token last;

    public Lexer(string source, string file)
    {
        this.src = source ?? "";
        this.file = file ?? "<input>";
    }

    public List<Token> Lex()
    {
        var tokens = new List<Token>();
        while (true)
        {
            var t = NextToken();
            tokens.Add(t);
            if (t.Type == TokenType.EOF) break;
        }
        return tokens;
    }

    private char Peek(int offset = 0)
    {
        int p = pos + offset;
        return p < src.Length ? src[p] : '\0';
    }

    private void Advance(int count = 1)
    {
        for (int i = 0; i < count; i++)
        {
            if (pos >= src.Length) return;

            if (src[pos] == '\n')
            {
                line++;
                col = 1;
            }
            else
            {
                col++;
            }
            pos++;
        }
    }

    private Token Emit(TokenType type, string text, int sl, int sc)
    {
        var t = new Token(type, text, sl, sc);
        last = t;
        return t;
    }

    private void SkipSpacesAndComments()
    {
        while (pos < src.Length)
        {
            char c = Peek();
            if (c == ' ' || c == '\t' || c == '\v' || c == '\f')
            {
                Advance();
            }
            else if (c == '#')
            {
                while (pos < src.Length && Peek() != '\n' && Peek() != '\r')
                    Advance();
            }
            else
            {
                break;
            }
        }
    }

    private Token NextToken()
    {
        SkipSpacesAndComments();

        if (pos >= src.Length)
            return Emit(TokenType.EOF, "", line, col);

        int sl = line;
        int sc = col;
        char c = Peek();

        if (c == '\n')
        {
            Advance();
            return Emit(TokenType.Newline, "\\n", sl, sc);
        }

        if (c == '\r')
        {
            Advance();
            if (Peek() == '\n') Advance();
            return Emit(TokenType.Newline, "\\n", sl, sc);
        }

        if (char.IsDigit(c) || (c == '.' && char.IsDigit(Peek(1))))
            return ReadNumber();

        if (c == '"' || c == '\'')
            return ReadString();

        if (char.IsLetter(c) || c == '_')
            return ReadWordOrIdentifier(stopAtEquals: true);

        if (c == '=')
        {
            Advance();
            if (Peek() == '=')
            {
                Advance();
                return Emit(TokenType.Eq, "==", sl, sc);
            }
            return Emit(TokenType.Assign, "=", sl, sc);
        }

        if (c == '!')
        {
            Advance();
            if (Peek() == '=')
            {
                Advance();
                return Emit(TokenType.NotEq, "!=", sl, sc);
            }
            return Emit(TokenType.Word, "!", sl, sc);
        }

        if (c == '~')
        {
            Advance();
            if (Peek() == '=')
            {
                Advance();
                return Emit(TokenType.NotEq, "~=", sl, sc);
            }
            return Emit(TokenType.Word, "~", sl, sc);
        }

        if (c == '<')
        {
            Advance();
            if (Peek() == '=')
            {
                Advance();
                return Emit(TokenType.LtEq, "<=", sl, sc);
            }
            return Emit(TokenType.Lt, "<", sl, sc);
        }

        if (c == '>')
        {
            Advance();
            if (Peek() == '>')
            {
                Advance();
                return Emit(TokenType.GtGt, ">>", sl, sc);
            }
            if (Peek() == '=')
            {
                Advance();
                return Emit(TokenType.GtEq, ">=", sl, sc);
            }
            return Emit(TokenType.Gt, ">", sl, sc);
        }

        if (c == '+')
        {
            Advance();
            return Emit(TokenType.Plus, "+", sl, sc);
        }

        if (c == '-')
        {
            if (char.IsDigit(Peek(1)) && LastAllowsUnary())
                return ReadNumber();

            if (LooksLikeMinusOperator())
            {
                Advance();
                return Emit(TokenType.Minus, "-", sl, sc);
            }

            return ReadWord(stopAtEquals: false);
        }

        if (c == '*')
        {
            if (LooksLikeStarOperator())
            {
                Advance();
                return Emit(TokenType.Star, "*", sl, sc);
            }
            return ReadWord(stopAtEquals: false);
        }

        if (c == '/')
        {
            if (LooksLikeSlashOperator())
            {
                Advance();
                return Emit(TokenType.Slash, "/", sl, sc);
            }
            return ReadWord(stopAtEquals: false);
        }

        if (c == '%')
        {
            Advance();
            return Emit(TokenType.Percent, "%", sl, sc);
        }

        if (c == '.')
        {
            if (Peek() == '.')
            {
                Advance();
                Advance();
                return Emit(TokenType.DotDot, "..", sl, sc);
            }
            return ReadWord(stopAtEquals: false);
        }

        if (c == '(')
        {
            Advance();
            return Emit(TokenType.LParen, "(", sl, sc);
        }

        if (c == ')')
        {
            Advance();
            return Emit(TokenType.RParen, ")", sl, sc);
        }

        if (c == ',')
        {
            Advance();
            return Emit(TokenType.Comma, ",", sl, sc);
        }

        if (c == ';')
        {
            Advance();
            return Emit(TokenType.Semicolon, ";", sl, sc);
        }

        if (c == '|')
        {
            Advance();
            if (Peek() == '|')
            {
                Advance();
                return Emit(TokenType.OrOr, "||", sl, sc);
            }
            return Emit(TokenType.Pipe, "|", sl, sc);
        }

        if (c == '&')
        {
            Advance();
            if (Peek() == '&')
            {
                Advance();
                return Emit(TokenType.AmpAmp, "&&", sl, sc);
            }
            return Emit(TokenType.Word, "&", sl, sc);
        }

        return ReadWord(stopAtEquals: false);
    }

    private bool LastAllowsUnary()
    {
        if (last == null) return true;

        switch (last.Type)
        {
            case TokenType.Assign:
            case TokenType.Eq:
            case TokenType.NotEq:
            case TokenType.Lt:
            case TokenType.LtEq:
            case TokenType.Gt:
            case TokenType.GtEq:
            case TokenType.Plus:
            case TokenType.Minus:
            case TokenType.Star:
            case TokenType.Slash:
            case TokenType.Percent:
            case TokenType.DotDot:
            case TokenType.LParen:
            case TokenType.Comma:
            case TokenType.Pipe:
            case TokenType.AmpAmp:
            case TokenType.OrOr:
            case TokenType.Semicolon:
            case TokenType.Newline:
                return true;

            case TokenType.Keyword:
                return last.Text is "then" or "do" or "in" or "return" or "and" or "or" or "not" or "else";

            default:
                return false;
        }
    }

    private bool LooksLikeMinusOperator()
    {
        char n = Peek(1);
        return n == '\0' || char.IsWhiteSpace(n) || n == ')' || n == ',' || n == ';' || n == '|' || n == '&';
    }

    private bool LooksLikeStarOperator()
    {
        char n = Peek(1);
        return n == '\0' || char.IsWhiteSpace(n) || n == ')' || n == ',' || n == ';' || n == '|' || n == '&';
    }

    private bool LooksLikeSlashOperator()
    {
        char n = Peek(1);
        return n == '\0' || char.IsWhiteSpace(n) || n == ')' || n == ',' || n == ';' || n == '|' || n == '&';
    }

    private Token ReadNumber()
    {
        int sl = line;
        int sc = col;
        int start = pos;

        if (Peek() == '-') Advance();

        if (Peek() == '.')
        {
            Advance();
            while (char.IsDigit(Peek())) Advance();
        }
        else
        {
            while (char.IsDigit(Peek())) Advance();

            if (Peek() == '.' && char.IsDigit(Peek(1)))
            {
                Advance();
                while (char.IsDigit(Peek())) Advance();
            }
        }

        string text = src.Substring(start, pos - start);
        return Emit(TokenType.Number, text, sl, sc);
    }

    private Token ReadString()
    {
        int sl = line;
        int sc = col;
        char quote = Peek();
        Advance();

        var sb = new StringBuilder();

        while (pos < src.Length && Peek() != quote)
        {
            char c = Peek();

            if (c == '\n' || c == '\r')
                throw new JashError("unterminated string", new Token(TokenType.String, "", sl, sc)) { File = file };

            if (c == '\\')
            {
                Advance();
                char esc = Peek();
                switch (esc)
                {
                    case 'n': sb.Append('\n'); break;
                    case 't': sb.Append('\t'); break;
                    case 'r': sb.Append('\r'); break;
                    case '"': sb.Append('"'); break;
                    case '\'': sb.Append('\''); break;
                    case '\\': sb.Append('\\'); break;
                    case '0': sb.Append('\0'); break;
                    default: sb.Append(esc); break;
                }
                Advance();
            }
            else
            {
                sb.Append(c);
                Advance();
            }
        }

        if (pos >= src.Length)
            throw new JashError("unterminated string", new Token(TokenType.String, "", sl, sc)) { File = file };

        Advance(); // closing quote
        return Emit(TokenType.String, sb.ToString(), sl, sc);
    }

    private Token ReadWordOrIdentifier(bool stopAtEquals)
    {
        int sl = line;
        int sc = col;
        string text = ReadWordCore(stopAtEquals);

        if (IsPlainIdentifier(text))
        {
            if (Keywords.Contains(text))
                return Emit(TokenType.Keyword, text, sl, sc);
            return Emit(TokenType.Identifier, text, sl, sc);
        }

        return Emit(TokenType.Word, text, sl, sc);
    }

    private Token ReadWord(bool stopAtEquals)
    {
        int sl = line;
        int sc = col;
        string text = ReadWordCore(stopAtEquals);

        if (text.Length == 0)
        {
            char c = Peek();
            Advance();
            text = c.ToString();
        }

        return Emit(TokenType.Word, text, sl, sc);
    }

    private string ReadWordCore(bool stopAtEquals)
    {
        var sb = new StringBuilder();

        while (pos < src.Length)
        {
            char c = Peek();

            if (c == '\n' || c == '\r' || char.IsWhiteSpace(c))
                break;

            if (stopAtEquals && c == '=')
                break;

            if (c == '|' || c == '&' || c == ';' || c == '<' || c == '>' ||
                c == '(' || c == ')' || c == ',' || c == '"' || c == '\'' || c == '#')
                break;

            if (c == '.' && Peek(1) == '.')
                break;

            sb.Append(c);
            Advance();
        }

        return sb.ToString();
    }

    private static bool IsPlainIdentifier(string text)
    {
        if (string.IsNullOrEmpty(text)) return false;

        foreach (char c in text)
        {
            if (!(char.IsLetterOrDigit(c) || c == '_'))
                return false;
        }

        return true;
    }
}

public abstract class Stmt
{
    public Token Token;
}

public abstract class Expr
{
    public Token Token;
}

public class AstProgram
{
    public List<Stmt> Statements = new();
}

public class AssignStmt : Stmt
{
    public string Name;
    public Expr Value;
}

public class ExprStmt : Stmt
{
    public Expr Expression;
}

public class IfStmt : Stmt
{
    public Expr Cond;
    public List<Stmt> ThenBody = new();
    public List<ElseIfClause> ElseIfs = new();
    public List<Stmt> ElseBody;
}

public class ElseIfClause
{
    public Expr Cond;
    public List<Stmt> Body = new();
}

public class WhileStmt : Stmt
{
    public Expr Cond;
    public List<Stmt> Body = new();
}

public class NumericForStmt : Stmt
{
    public string Var;
    public Expr Start;
    public Expr Limit;
    public Expr Step;
    public List<Stmt> Body = new();
}

public class ForInStmt : Stmt
{
    public string Var;
    public Expr Pattern;
    public List<Stmt> Body = new();
}

public class FunctionStmt : Stmt
{
    public string Name;
    public List<string> Params = new();
    public List<Stmt> Body = new();
}

public class ReturnStmt : Stmt
{
    public Expr Value;
}

public class BreakStmt : Stmt
{
}

public class CommandStmt : Stmt
{
    public CommandChain Chain;
}

public class CommandChain
{
    public Token Token;
    public List<ChainPart> Parts = new();
}

public class ChainPart
{
    public string Op; // "", "&&", "||"
    public Pipeline Pipeline;
}

public class Pipeline
{
    public List<CommandSegment> Segments = new();
}

public class CommandSegment
{
    public Token CommandToken;
    public string Command;
    public List<CommandArg> Args = new();
    public CommandArg StdinFile;
    public CommandArg StdoutFile;
    public bool StdoutAppend;
}

public class CommandArg
{
    public Token Token;
    public string Text;
    public bool Quoted;
}

public class NumberLiteral : Expr
{
    public double Value;
}

public class StringLiteral : Expr
{
    public string Value;
}

public class BoolLiteral : Expr
{
    public bool Value;
}

public class NilLiteral : Expr
{
}

public class VariableExpr : Expr
{
    public string Name;
}

public class BinaryExpr : Expr
{
    public string Op;
    public Expr Left;
    public Expr Right;
}

public class UnaryExpr : Expr
{
    public string Op;
    public Expr Operand;
}

public class CallExpr : Expr
{
    public string Name;
    public List<Expr> Args = new();
}

public class Parser
{
    private readonly List<Token> tokens;
    private readonly string file;
    private int pos;

    private Parser(string source, string file)
    {
        this.file = file;
        tokens = new Lexer(source, file).Lex();
    }

    public static AstProgram Parse(string source, string file)
    {
        return new Parser(source, file).ParseProgram();
    }

    private Token Current => pos < tokens.Count ? tokens[pos] : tokens[^1];

    private Token Peek(int offset = 1)
    {
        int idx = pos + offset;
        return idx < tokens.Count ? tokens[idx] : tokens[^1];
    }

    private Token Advance()
    {
        var t = Current;
        if (pos < tokens.Count - 1) pos++;
        return t;
    }

    private bool Check(TokenType type) => Current.Type == type;

    private bool CheckKeyword(string text) => Current.Type == TokenType.Keyword && Current.Text == text;

    private bool Match(TokenType type)
    {
        if (!Check(type)) return false;
        Advance();
        return true;
    }

    private bool MatchKeyword(string text)
    {
        if (!CheckKeyword(text)) return false;
        Advance();
        return true;
    }

    private Token Expect(TokenType type, string message)
    {
        if (Check(type)) return Advance();
        ThrowJash(message, Current, Current.Type == TokenType.EOF);
        return null;
    }

    private Token ExpectKeyword(string keyword, string message)
    {
        if (CheckKeyword(keyword)) return Advance();
        ThrowJash(message, Current, Current.Type == TokenType.EOF);
        return null;
    }

    private void ThrowJash(string message, Token token = null, bool incomplete = false)
    {
        token ??= Current;
        throw new JashError(message, token)
        {
            File = file,
            Incomplete = incomplete
        };
    }

    private void SkipSeparators()
    {
        while (Current.Type == TokenType.Newline || Current.Type == TokenType.Semicolon)
            Advance();
    }

    private AstProgram ParseProgram()
    {
        var program = new AstProgram();

        while (true)
        {
            SkipSeparators();

            if (Current.Type == TokenType.EOF)
                break;

            var stmt = ParseStatement();
            if (stmt != null)
                program.Statements.Add(stmt);
        }

        return program;
    }

    private Stmt ParseStatement()
    {
        if (Current.Type == TokenType.Newline || Current.Type == TokenType.Semicolon)
        {
            Advance();
            return null;
        }

        if (Current.Type == TokenType.Keyword)
        {
            switch (Current.Text)
            {
                case "if": return ParseIf();
                case "while": return ParseWhile();
                case "for": return ParseFor();
                case "function": return ParseFunction();
                case "return": return ParseReturn();
                case "break": return ParseBreak();

                case "end":
                case "else":
                case "elseif":
                    ThrowJash($"unexpected '{Current.Text}'", Current);
                    break;
            }
        }

        if (Current.Type == TokenType.Identifier && Peek(1).Type == TokenType.Assign)
        {
            var name = Advance();
            Advance(); // =
            var value = ParseExpression();
            return new AssignStmt { Token = name, Name = name.Text, Value = value };
        }

        if (CanStartExpression())
        {
            var expr = ParseExpression();
            return new ExprStmt { Token = expr.Token, Expression = expr };
        }

        var chain = ParseCommandChain();
        return new CommandStmt { Token = chain.Token, Chain = chain };
    }

    private bool CanStartExpression()
    {
        if (Current.Type == TokenType.Number) return true;
        if (Current.Type == TokenType.String) return true;
        if (Current.Type == TokenType.LParen) return true;
        if (Current.Type == TokenType.Identifier && Peek(1).Type == TokenType.LParen) return true;
        if (Current.Type == TokenType.Keyword && Current.Text == "not") return true;
        return false;
    }

    private Expr ParseExpression() => ParseOr();

    private Expr ParseOr()
    {
        var left = ParseAnd();

        while (CheckKeyword("or"))
        {
            var op = Advance();
            var right = ParseAnd();
            left = new BinaryExpr { Token = op, Op = "or", Left = left, Right = right };
        }

        return left;
    }

    private Expr ParseAnd()
    {
        var left = ParseComparison();

        while (CheckKeyword("and"))
        {
            var op = Advance();
            var right = ParseComparison();
            left = new BinaryExpr { Token = op, Op = "and", Left = left, Right = right };
        }

        return left;
    }

    private Expr ParseComparison()
    {
        var left = ParseConcat();

        while (Current.Type is TokenType.Eq or TokenType.NotEq or TokenType.Lt or TokenType.LtEq or TokenType.Gt or TokenType.GtEq)
        {
            var op = Advance();
            var right = ParseConcat();

            string opText = op.Text;
            if (op.Type == TokenType.NotEq)
                opText = "!=";

            left = new BinaryExpr { Token = op, Op = opText, Left = left, Right = right };
        }

        return left;
    }

    private Expr ParseConcat()
    {
        var left = ParseAdditive();

        while (Check(TokenType.DotDot))
        {
            var op = Advance();
            var right = ParseAdditive();
            left = new BinaryExpr { Token = op, Op = "..", Left = left, Right = right };
        }

        return left;
    }

    private Expr ParseAdditive()
    {
        var left = ParseMultiplicative();

        while (Current.Type is TokenType.Plus or TokenType.Minus)
        {
            var op = Advance();
            var right = ParseMultiplicative();
            left = new BinaryExpr { Token = op, Op = op.Text, Left = left, Right = right };
        }

        return left;
    }

    private Expr ParseMultiplicative()
    {
        var left = ParseUnary();

        while (Current.Type is TokenType.Star or TokenType.Slash or TokenType.Percent)
        {
            var op = Advance();
            var right = ParseUnary();
            left = new BinaryExpr { Token = op, Op = op.Text, Left = left, Right = right };
        }

        return left;
    }

    private Expr ParseUnary()
    {
        if (CheckKeyword("not"))
        {
            var op = Advance();
            var operand = ParseUnary();
            return new UnaryExpr { Token = op, Op = "not", Operand = operand };
        }

        if (Check(TokenType.Minus))
        {
            var op = Advance();
            var operand = ParseUnary();
            return new UnaryExpr { Token = op, Op = "-", Operand = operand };
        }

        return ParsePrimary();
    }

    private Expr ParsePrimary()
    {
        var t = Current;

        switch (t.Type)
        {
            case TokenType.Number:
                Advance();
                double.TryParse(t.Text, NumberStyles.Any, CultureInfo.InvariantCulture, out double v);
                return new NumberLiteral { Token = t, Value = v };

            case TokenType.String:
                Advance();
                return new StringLiteral { Token = t, Value = t.Text };

            case TokenType.Keyword when t.Text == "true":
                Advance();
                return new BoolLiteral { Token = t, Value = true };

            case TokenType.Keyword when t.Text == "false":
                Advance();
                return new BoolLiteral { Token = t, Value = false };

            case TokenType.Keyword when t.Text == "nil":
                Advance();
                return new NilLiteral { Token = t };

            case TokenType.Identifier:
                Advance();
                if (Current.Type == TokenType.LParen)
                {
                    Advance();
                    var args = ParseCallArgs();
                    return new CallExpr { Token = t, Name = t.Text, Args = args };
                }
                return new VariableExpr { Token = t, Name = t.Text };

            case TokenType.LParen:
                Advance();
                var expr = ParseExpression();
                Expect(TokenType.RParen, "expected ')'");
                return expr;

            case TokenType.EOF:
                ThrowJash("unexpected end of input", t, true);
                break;

            default:
                ThrowJash($"unexpected token '{t.Text}'", t);
                break;
        }

        return null;
    }

    private List<Expr> ParseCallArgs()
    {
        var args = new List<Expr>();

        SkipSeparators();

        if (Check(TokenType.RParen))
        {
            Advance();
            return args;
        }

        while (true)
        {
            args.Add(ParseExpression());
            SkipSeparators();

            if (Match(TokenType.Comma))
            {
                SkipSeparators();
                continue;
            }

            break;
        }

        Expect(TokenType.RParen, "expected ')'");
        return args;
    }

    private CommandChain ParseCommandChain()
    {
        var chain = new CommandChain { Token = Current };
        string op = "";

        while (true)
        {
            var pipeline = ParsePipeline();
            chain.Parts.Add(new ChainPart { Op = op, Pipeline = pipeline });

            if (Current.Type == TokenType.AmpAmp)
            {
                op = "&&";
                Advance();
                SkipSeparators();
                continue;
            }

            if (Current.Type == TokenType.OrOr)
            {
                op = "||";
                Advance();
                SkipSeparators();
                continue;
            }

            break;
        }

        return chain;
    }

    private Pipeline ParsePipeline()
    {
        var pipeline = new Pipeline();
        pipeline.Segments.Add(ParseCommandSegment());

        while (Check(TokenType.Pipe))
        {
            Advance();
            pipeline.Segments.Add(ParseCommandSegment());
        }

        return pipeline;
    }

    private CommandSegment ParseCommandSegment()
    {
        if (!IsCommandStart(Current))
        {
            ThrowJash("expected command", Current, Current.Type == TokenType.EOF);
        }

        var seg = new CommandSegment();
        var cmd = Advance();
        seg.CommandToken = cmd;
        seg.Command = cmd.Text;

        while (true)
        {
            var t = Current;

            if (t.Type is TokenType.EOF or TokenType.Newline or TokenType.Semicolon or TokenType.Pipe or TokenType.AmpAmp or TokenType.OrOr)
                break;

            if (t.Type is TokenType.Gt or TokenType.GtGt or TokenType.Lt)
            {
                ParseRedirect(seg);
                continue;
            }

            if (t.Type == TokenType.Keyword && t.Text is "end" or "else" or "elseif" or "then" or "do")
                break;

            if (IsCommandArgToken(t))
            {
                seg.Args.Add(new CommandArg
                {
                    Token = t,
                    Text = t.Text,
                    Quoted = t.Type == TokenType.String
                });
                Advance();
                continue;
            }

            ThrowJash($"unexpected token '{t.Text}' in command", t);
        }

        return seg;
    }

    private bool IsCommandStart(Token t)
    {
        if (t.Type is TokenType.Identifier or TokenType.Word or TokenType.String)
            return true;

        if (t.Type == TokenType.Keyword && t.Text is not ("end" or "else" or "elseif" or "then" or "do"))
            return true;

        return false;
    }

    private bool IsCommandArgToken(Token t)
    {
        switch (t.Type)
        {
            case TokenType.Identifier:
            case TokenType.Word:
            case TokenType.Number:
            case TokenType.String:
            case TokenType.Assign:
            case TokenType.Eq:
            case TokenType.NotEq:
            case TokenType.Plus:
            case TokenType.Minus:
            case TokenType.Star:
            case TokenType.Slash:
            case TokenType.Percent:
            case TokenType.DotDot:
                return true;

            case TokenType.Keyword:
                return t.Text is not ("end" or "else" or "elseif" or "then" or "do" or
                                      "if" or "while" or "for" or "function" or "return" or "break");

            default:
                return false;
        }
    }

    private void ParseRedirect(CommandSegment seg)
    {
        var op = Advance();

        if (!IsFileNameToken(Current))
            ThrowJash("expected file name after redirection", Current, Current.Type == TokenType.EOF);

        var arg = new CommandArg
        {
            Token = Current,
            Text = Current.Text,
            Quoted = Current.Type == TokenType.String
        };

        Advance();

        if (op.Type == TokenType.Lt)
        {
            seg.StdinFile = arg;
        }
        else if (op.Type == TokenType.Gt)
        {
            seg.StdoutFile = arg;
            seg.StdoutAppend = false;
        }
        else
        {
            seg.StdoutFile = arg;
            seg.StdoutAppend = true;
        }
    }

    private bool IsFileNameToken(Token t)
    {
        return t.Type is TokenType.Identifier or TokenType.Word or TokenType.String or TokenType.Number;
    }

    private IfStmt ParseIf()
    {
        var tok = ExpectKeyword("if", "expected 'if'");
        var cond = ParseExpression();
        ExpectKeyword("then", "expected 'then' after if condition");

        var stmt = new IfStmt
        {
            Token = tok,
            Cond = cond,
            ThenBody = ParseBlock(IsIfTerm)
        };

        while (CheckKeyword("elseif"))
        {
            Advance();
            var elseifCond = ParseExpression();
            ExpectKeyword("then", "expected 'then' after elseif condition");

            stmt.ElseIfs.Add(new ElseIfClause
            {
                Cond = elseifCond,
                Body = ParseBlock(IsIfTerm)
            });
        }

        if (MatchKeyword("else"))
        {
            stmt.ElseBody = ParseBlock(t => t.Type == TokenType.Keyword && t.Text == "end");
        }

        ExpectKeyword("end", "expected 'end' to close if");
        return stmt;
    }

    private static bool IsIfTerm(Token t)
    {
        return t.Type == TokenType.Keyword && t.Text is "end" or "else" or "elseif";
    }

    private WhileStmt ParseWhile()
    {
        var tok = ExpectKeyword("while", "expected 'while'");
        var cond = ParseExpression();
        ExpectKeyword("do", "expected 'do' after while condition");

        var body = ParseBlock(t => t.Type == TokenType.Keyword && t.Text == "end");
        ExpectKeyword("end", "expected 'end' to close while");

        return new WhileStmt { Token = tok, Cond = cond, Body = body };
    }

    private Stmt ParseFor()
    {
        var tok = ExpectKeyword("for", "expected 'for'");
        var varToken = Expect(TokenType.Identifier, "expected variable name after 'for'");

        if (Match(TokenType.Assign))
        {
            var start = ParseExpression();
            Expect(TokenType.Comma, "expected ',' in numeric for");

            var limit = ParseExpression();
            Expr step = null;

            if (Match(TokenType.Comma))
                step = ParseExpression();

            ExpectKeyword("do", "expected 'do' after for range");

            var body = ParseBlock(t => t.Type == TokenType.Keyword && t.Text == "end");
            ExpectKeyword("end", "expected 'end' to close for");

            return new NumericForStmt
            {
                Token = tok,
                Var = varToken.Text,
                Start = start,
                Limit = limit,
                Step = step,
                Body = body
            };
        }

        if (MatchKeyword("in"))
        {
            var pattern = ParseExpression();
            ExpectKeyword("do", "expected 'do' after for-in pattern");

            var body = ParseBlock(t => t.Type == TokenType.Keyword && t.Text == "end");
            ExpectKeyword("end", "expected 'end' to close for");

            return new ForInStmt
            {
                Token = tok,
                Var = varToken.Text,
                Pattern = pattern,
                Body = body
            };
        }

        ThrowJash("expected '=' or 'in' in for statement", Current);
        return null;
    }

    private FunctionStmt ParseFunction()
    {
        var tok = ExpectKeyword("function", "expected 'function'");
        var name = Expect(TokenType.Identifier, "expected function name");

        Expect(TokenType.LParen, "expected '(' after function name");

        var pars = new List<string>();
        SkipSeparators();

        if (!Check(TokenType.RParen))
        {
            while (true)
            {
                var p = Expect(TokenType.Identifier, "expected parameter name");
                pars.Add(p.Text);
                SkipSeparators();

                if (Match(TokenType.Comma))
                {
                    SkipSeparators();
                    continue;
                }

                break;
            }
        }

        Expect(TokenType.RParen, "expected ')' after function parameters");

        var body = ParseBlock(t => t.Type == TokenType.Keyword && t.Text == "end");
        ExpectKeyword("end", "expected 'end' to close function");

        return new FunctionStmt
        {
            Token = tok,
            Name = name.Text,
            Params = pars,
            Body = body
        };
    }

    private ReturnStmt ParseReturn()
    {
        var tok = Advance(); // return
        Expr value = null;

        bool noValue =
            Check(TokenType.Newline) ||
            Check(TokenType.Semicolon) ||
            Check(TokenType.EOF) ||
            CheckKeyword("end") ||
            CheckKeyword("else") ||
            CheckKeyword("elseif");

        if (!noValue)
            value = ParseExpression();

        return new ReturnStmt { Token = tok, Value = value };
    }

    private BreakStmt ParseBreak()
    {
        var tok = Advance();
        return new BreakStmt { Token = tok };
    }

    private List<Stmt> ParseBlock(Func<Token, bool> isTerminator)
    {
        var list = new List<Stmt>();

        while (true)
        {
            SkipSeparators();

            if (Current.Type == TokenType.EOF)
                ThrowJash("unexpected end of input: expected block terminator", Current, true);

            if (isTerminator(Current))
                break;

            var stmt = ParseStatement();
            if (stmt != null)
                list.Add(stmt);
        }

        return list;
    }
}

public static class Values
{
    public static bool Truthy(object v)
    {
        if (v == null) return false;
        if (v is bool b) return b;
        return true;
    }

    public static string ToStringValue(object v)
    {
        return v switch
        {
            null => "nil",
            bool b => b ? "true" : "false",
            double d => FormatNumber(d),
            string s => s,
            _ => v.ToString()
        };
    }

    public static string FormatNumber(double d)
    {
        if (double.IsNaN(d)) return "nan";
        if (double.IsPositiveInfinity(d)) return "inf";
        if (double.IsNegativeInfinity(d)) return "-inf";

        if (d == Math.Floor(d) && Math.Abs(d) < 1e15)
            return ((long)d).ToString(CultureInfo.InvariantCulture);

        return d.ToString("R", CultureInfo.InvariantCulture);
    }

    public static bool TryToNumber(object v, out double number)
    {
        switch (v)
        {
            case null:
                number = 0;
                return false;

            case double d:
                number = d;
                return true;

            case bool b:
                number = b ? 1 : 0;
                return true;

            case string s:
                return double.TryParse(s.Trim(), NumberStyles.Any, CultureInfo.InvariantCulture, out number);

            default:
                number = 0;
                return false;
        }
    }

    public static bool ValueEquals(object a, object b)
    {
        if (a == null && b == null) return true;
        if (a == null || b == null) return false;

        if (a is bool ba && b is bool bb)
            return ba == bb;

        if (a is string sa && b is string sb)
            return string.Equals(sa, sb, StringComparison.Ordinal);

        if (TryToNumber(a, out double da) && TryToNumber(b, out double db))
        {
            if (a is double || b is double)
                return Math.Abs(da - db) < 1e-12;
        }

        return a.Equals(b);
    }

    public static int Compare(object a, object b, Token token = null)
    {
        bool an = TryToNumber(a, out double da);
        bool bn = TryToNumber(b, out double db);

        if (an && bn && !(a is string && b is string))
            return da.CompareTo(db);

        if (a is string sa && b is string sb)
            return string.CompareOrdinal(sa, sb);

        if (an && bn)
            return da.CompareTo(db);

        throw new JashError($"cannot compare {TypeName(a)} and {TypeName(b)}", token);
    }

    public static string TypeName(object v)
    {
        return v switch
        {
            null => "nil",
            bool => "boolean",
            double => "number",
            string => "string",
            _ => "object"
        };
    }
}

public class RuntimeEnvironment
{
    private readonly List<Dictionary<string, object>> scopes = new()
    {
        new Dictionary<string, object>(StringComparer.OrdinalIgnoreCase)
    };

    public void PushScope()
    {
        scopes.Add(new Dictionary<string, object>(StringComparer.OrdinalIgnoreCase));
    }

    public void PopScope()
    {
        if (scopes.Count > 1)
            scopes.RemoveAt(scopes.Count - 1);
    }

    public bool TryGet(string name, out object value)
    {
        for (int i = scopes.Count - 1; i >= 0; i--)
        {
            if (scopes[i].TryGetValue(name, out value))
                return true;
        }

        value = null;
        return false;
    }

    public void Set(string name, object value)
    {
        for (int i = scopes.Count - 1; i >= 0; i--)
        {
            if (scopes[i].ContainsKey(name))
            {
                scopes[i][name] = value;
                return;
            }
        }

        scopes[0][name] = value;
    }

    public void Define(string name, object value)
    {
        scopes[^1][name] = value;
    }

    public IEnumerable<string> Names =>
        scopes.SelectMany(d => d.Keys).Distinct(StringComparer.OrdinalIgnoreCase);
}

public class JashFunction
{
    public string Name;
    public List<string> Params = new();
    public List<Stmt> Body = new();
}

public class Runtime
{
    public string Source = "<input>";
    public string Cwd;

    public TextReader StdIn;
    public TextWriter StdOut;
    public TextWriter StdErr;

    public RuntimeEnvironment Vars = new();
    public Dictionary<string, JashFunction> Functions = new(StringComparer.OrdinalIgnoreCase);
    public Dictionary<string, Func<List<object>, object>> NativeFunctions = new(StringComparer.OrdinalIgnoreCase);
    public Dictionary<string, string> EnvVars = new(StringComparer.OrdinalIgnoreCase);

    public ShellEngine Shell;
    public IJetOSApi JetOS;

    public int LastExitCode;
    public bool ExitRequested;
    public int RequestedExitCode;

    public List<string> History = new();

    public Runtime(IJetOSApi jetOS)
    {
        JetOS = jetOS;
        Cwd = Directory.GetCurrentDirectory();

        StdIn = Console.In;
        StdOut = Console.Out;
        StdErr = Console.Error;

        foreach (DictionaryEntry de in Environment.GetEnvironmentVariables())
        {
            if (de.Key == null) continue;
            EnvVars[de.Key.ToString()] = de.Value?.ToString() ?? "";
        }

        Shell = new ShellEngine(this);
        Builtins.RegisterAll(this, Shell, JetOS);
    }

    public string GetSuggestion(string name)
    {
        if (string.IsNullOrEmpty(name)) return null;

        var candidates = Shell.CommandNames
            .Concat(NativeFunctions.Keys)
            .Concat(Functions.Keys)
            .Concat(Vars.Names)
            .Distinct(StringComparer.OrdinalIgnoreCase);

        return StringSimilarity.Best(name, candidates);
    }
}

public class CommandContext
{
    public Runtime Runtime;
    public string CommandName;
    public List<string> Args = new();
    public TextReader In;
    public TextWriter Out;
    public TextWriter Err;
}

public class ShellEngine
{
    private readonly Runtime rt;
    private readonly Dictionary<string, Func<CommandContext, int>> commands =
        new(StringComparer.OrdinalIgnoreCase);

    public ShellEngine(Runtime runtime)
    {
        rt = runtime;
    }

    public IEnumerable<string> CommandNames => commands.Keys;

    public void Register(string name, Func<CommandContext, int> handler)
    {
        commands[name] = handler;
    }

    public bool TryGetBuiltin(string name, out Func<CommandContext, int> handler)
    {
        return commands.TryGetValue(name, out handler);
    }

    public bool HasBuiltin(string name)
    {
        return commands.ContainsKey(name);
    }

    public bool HasCommandOrExternal(string name)
    {
        if (string.IsNullOrWhiteSpace(name)) return false;
        if (commands.ContainsKey(name)) return true;
        return ResolveExternal(name) != null;
    }

    public int ExecutePipeline(Pipeline pipeline)
    {
        return PipelineExecutor.Execute(rt, this, pipeline);
    }

    public string EvaluateArg(CommandArg arg)
    {
        if (arg == null) return "";
        if (arg.Quoted) return arg.Text ?? "";

        string text = arg.Text ?? "";

        if (rt.Vars.TryGet(text, out object exact))
            return Values.ToStringValue(exact);

        return Interpolate(text);
    }

    private string Interpolate(string s)
    {
        if (string.IsNullOrEmpty(s) || s.IndexOf('{') < 0)
            return s;

        var sb = new StringBuilder();

        for (int i = 0; i < s.Length; i++)
        {
            if (s[i] == '{')
            {
                int end = s.IndexOf('}', i + 1);
                if (end > i)
                {
                    string name = s.Substring(i + 1, end - i - 1);
                    object val = null;

                    if (name.StartsWith("env:", StringComparison.OrdinalIgnoreCase))
                    {
                        string envName = name.Substring(4);
                        if (rt.EnvVars.TryGetValue(envName, out string ev))
                            val = ev;
                    }
                    else
                    {
                        if (rt.Vars.TryGet(name, out object v))
                            val = v;
                        else if (rt.EnvVars.TryGetValue(name, out string envVal))
                            val = envVal;
                    }

                    sb.Append(val == null ? "" : Values.ToStringValue(val));
                    i = end;
                    continue;
                }
            }

            sb.Append(s[i]);
        }

        return sb.ToString();
    }

    public string ResolvePath(string path)
    {
        if (string.IsNullOrWhiteSpace(path))
            return rt.Cwd;

        try
        {
            if (Path.IsPathRooted(path))
                return Path.GetFullPath(path);

            return Path.GetFullPath(Path.Combine(rt.Cwd, path));
        }
        catch
        {
            return Path.Combine(rt.Cwd, path);
        }
    }

    public List<string> ExpandPatterns(string pattern, bool filesOnly = false)
    {
        var results = new List<string>();

        if (string.IsNullOrEmpty(pattern))
            return results;

        string full = ResolvePath(pattern);

        if (pattern.Contains('*') || pattern.Contains('?'))
        {
            string dir = Path.GetDirectoryName(full);
            if (string.IsNullOrEmpty(dir)) dir = rt.Cwd;

            string filePat = Path.GetFileName(full);
            if (string.IsNullOrEmpty(filePat)) filePat = "*";

            if (Directory.Exists(dir))
            {
                foreach (var p in Directory.EnumerateFileSystemEntries(dir, filePat, SearchOption.TopDirectoryOnly).OrderBy(x => x))
                {
                    if (filesOnly && !File.Exists(p))
                        continue;

                    results.Add(p);
                }
            }
        }
        else
        {
            results.Add(full);
        }

        return results;
    }

    public List<string> ExpandForIn(string pattern)
    {
        if (string.IsNullOrWhiteSpace(pattern))
            return new List<string> { "" };

        bool hasWildcard = pattern.Contains('*') || pattern.Contains('?');

        if (!hasWildcard)
            return new List<string> { pattern };

        var fullPaths = ExpandPatterns(pattern);
        return fullPaths.Select(MakeRelative).ToList();
    }

    private string MakeRelative(string fullPath)
    {
        try
        {
            return Path.GetRelativePath(rt.Cwd, fullPath);
        }
        catch
        {
            return fullPath;
        }
    }

    public string ResolveExternal(string name)
    {
        if (string.IsNullOrWhiteSpace(name))
            return null;

        try
        {
            if (name.Contains(Path.DirectorySeparatorChar) || name.Contains(Path.AltDirectorySeparatorChar) || Path.IsPathRooted(name))
            {
                string p = ResolvePath(name);
                if (File.Exists(p)) return p;
                return null;
            }

            var searchDirs = new List<string> { rt.Cwd };

            if (rt.EnvVars.TryGetValue("PATH", out string pathVar))
            {
                searchDirs.AddRange(pathVar.Split(';', StringSplitOptions.RemoveEmptyEntries));
            }

            var pathExts = new List<string> { "" };

            if (rt.EnvVars.TryGetValue("PATHEXT", out string pathExt))
            {
                pathExts.AddRange(pathExt.Split(';', StringSplitOptions.RemoveEmptyEntries));
            }
            else
            {
                pathExts.AddRange(new[] { ".COM", ".EXE", ".BAT", ".CMD" });
            }

            bool hasExt = !string.IsNullOrEmpty(Path.GetExtension(name));

            foreach (var dir in searchDirs)
            {
                if (string.IsNullOrWhiteSpace(dir)) continue;

                try
                {
                    if (!Directory.Exists(dir)) continue;

                    if (hasExt)
                    {
                        string candidate = Path.Combine(dir, name);
                        if (File.Exists(candidate)) return candidate;
                    }
                    else
                    {
                        foreach (var ext in pathExts)
                        {
                            string candidate = Path.Combine(dir, ext.Length == 0 ? name : name + ext);
                            if (File.Exists(candidate)) return candidate;
                        }
                    }
                }
                catch
                {
                    // ignore malformed PATH entries
                }
            }
        }
        catch
        {
            return null;
        }

        return null;
    }

    public ProcessStartInfo CreateStartInfo(string file, List<string> args, bool redirectInput, bool redirectOutput, bool redirectError)
    {
        string fileName;
        string arguments;

        string ext = Path.GetExtension(file).ToLowerInvariant();

        if (ext == ".bat" || ext == ".cmd")
        {
            fileName = "cmd.exe";
            arguments = $"/c {QuoteArgument(file)} {QuoteArguments(args)}";
        }
        else
        {
            fileName = file;
            arguments = QuoteArguments(args);
        }

        var psi = new ProcessStartInfo
        {
            FileName = fileName,
            Arguments = arguments,
            UseShellExecute = false,
            WorkingDirectory = rt.Cwd,
            CreateNoWindow = true,
            RedirectStandardInput = redirectInput,
            RedirectStandardOutput = redirectOutput,
            RedirectStandardError = redirectError,
            StandardOutputEncoding = Encoding.UTF8,
            StandardErrorEncoding = Encoding.UTF8
        };

        try
        {
            psi.Environment.Clear();
            foreach (var kv in rt.EnvVars)
            {
                if (string.IsNullOrEmpty(kv.Key)) continue;
                psi.Environment[kv.Key] = kv.Value ?? "";
            }
        }
        catch
        {
            // If environment manipulation fails, keep default environment.
        }

        return psi;
    }

    public static string QuoteArguments(IEnumerable<string> args)
    {
        if (args == null) return "";
        return string.Join(" ", args.Select(QuoteArgument));
    }

    public static string QuoteArgument(string arg)
    {
        if (string.IsNullOrEmpty(arg))
            return "\"\"";

        bool needsQuote = arg.Any(c => c == ' ' || c == '\t' || c == '"');
        if (!needsQuote)
            return arg;

        var sb = new StringBuilder();
        sb.Append('"');

        int backslashes = 0;

        foreach (char c in arg)
        {
            if (c == '\\')
            {
                backslashes++;
                sb.Append(c);
            }
            else if (c == '"')
            {
                sb.Append('\\', backslashes * 2 + 1);
                sb.Append('"');
                backslashes = 0;
            }
            else
            {
                backslashes = 0;
                sb.Append(c);
            }
        }

        sb.Append('\\', backslashes * 2);
        sb.Append('"');

        return sb.ToString();
    }
}

public static class PipelineExecutor
{
    public static int Execute(Runtime rt, ShellEngine shell, Pipeline pipeline)
    {
        if (pipeline == null || pipeline.Segments.Count == 0)
            return 0;

        int n = pipeline.Segments.Count;
        var queues = new BlockingCollection<string>[Math.Max(0, n - 1)];

        for (int i = 0; i < queues.Length; i++)
            queues[i] = new BlockingCollection<string>();

        var tasks = new Task<int>[n];

        for (int i = 0; i < n; i++)
        {
            int idx = i;
            tasks[idx] = Task.Run(() => ExecuteStage(rt, shell, pipeline.Segments[idx], idx, n, queues));
        }

        try
        {
            Task.WaitAll(tasks);
        }
        catch
        {
            foreach (var q in queues)
                q.Dispose();
            return 1;
        }

        int exit = tasks[n - 1].Result;

        foreach (var t in tasks)
        {
            if (t.Result == 127)
            {
                exit = 127;
                break;
            }
        }

        foreach (var q in queues)
            q.Dispose();

        return exit;
    }

    private static int ExecuteStage(
        Runtime rt,
        ShellEngine shell,
        CommandSegment seg,
        int index,
        int count,
        BlockingCollection<string>[] queues)
    {
        TextReader input = null;
        TextWriter output = null;

        bool disposeInput = true;
        bool disposeOutput = true;

        try
        {
            if (seg.StdoutFile != null)
            {
                string outFile = shell.EvaluateArg(seg.StdoutFile);
                output = new StreamWriter(shell.ResolvePath(outFile), seg.StdoutAppend, Encoding.UTF8);
            }
            else if (index < count - 1)
            {
                output = new BlockingCollectionWriter(queues[index]);
            }
            else
            {
                output = rt.StdOut;
                disposeOutput = false;
            }

            if (seg.StdinFile != null)
            {
                string inFile = shell.EvaluateArg(seg.StdinFile);
                input = new StreamReader(shell.ResolvePath(inFile), Encoding.UTF8);
            }
            else if (index > 0)
            {
                input = new BlockingCollectionReader(queues[index - 1]);
            }
            else
            {
                input = rt.StdIn;
                disposeInput = false;
            }

            string cmdName = shell.EvaluateArg(new CommandArg { Text = seg.Command ?? "", Quoted = false });

            if (shell.TryGetBuiltin(cmdName, out var handler))
            {
                var args = seg.Args.Select(a => shell.EvaluateArg(a)).ToList();

                var ctx = new CommandContext
                {
                    Runtime = rt,
                    CommandName = cmdName,
                    Args = args,
                    In = input,
                    Out = output,
                    Err = rt.StdErr
                };

                int code;
                try
                {
                    code = handler(ctx);
                }
                catch (JashError e)
                {
                    rt.StdErr.WriteLine(e.Format());
                    code = 1;
                }
                catch (Exception e)
                {
                    rt.StdErr.WriteLine($"jash: {e.Message}");
                    code = 1;
                }

                output.Flush();
                return code;
            }
            else
            {
                string resolved = shell.ResolveExternal(cmdName);

                if (resolved == null)
                {
                    string suggestion = rt.GetSuggestion(cmdName);
                    var msg = $"jash: command not found: {cmdName}";
                    if (!string.IsNullOrEmpty(suggestion))
                        msg += $" (did you mean '{suggestion}'?)";

                    rt.StdErr.WriteLine(msg);
                    return 127;
                }

                var args = seg.Args.Select(a => shell.EvaluateArg(a)).ToList();
                return RunExternal(rt, shell, resolved, args, input, output, rt.StdErr);
            }
        }
        catch (JashError e)
        {
            rt.StdErr.WriteLine(e.Format());
            if (output == null && index < count - 1)
            {
                try { queues[index].CompleteAdding(); } catch { }
            }
            return 1;
        }
        catch (Exception e)
        {
            rt.StdErr.WriteLine($"jash: {e.Message}");
            if (output == null && index < count - 1)
            {
                try { queues[index].CompleteAdding(); } catch { }
            }
            return 1;
        }
        finally
        {
            if (disposeInput && input != null)
                input.Dispose();

            if (disposeOutput && output != null)
                output.Dispose();
        }
    }

    private static int RunExternal(
        Runtime rt,
        ShellEngine shell,
        string file,
        List<string> args,
        TextReader input,
        TextWriter output,
        TextWriter error)
    {
        bool inheritIn = ReferenceEquals(input, rt.StdIn) && ReferenceEquals(rt.StdIn, Console.In);
        bool inheritOut = ReferenceEquals(output, rt.StdOut) && ReferenceEquals(rt.StdOut, Console.Out);
        bool inheritErr = ReferenceEquals(error, rt.StdErr) && ReferenceEquals(rt.StdErr, Console.Error);

        var psi = shell.CreateStartInfo(file, args, !inheritIn, !inheritOut, !inheritErr);

        using var proc = new Process();
        proc.StartInfo = psi;

        try
        {
            proc.Start();
        }
        catch (Exception e)
        {
            error.WriteLine($"jash: failed to start '{file}': {e.Message}");
            return 126;
        }

        if (!inheritIn)
        {
            _ = Task.Run(() =>
            {
                try
                {
                    string line;
                    while ((line = input.ReadLine()) != null)
                    {
                        proc.StandardInput.WriteLine(line);
                    }
                }
                catch
                {
                    // Process may have exited or closed stdin.
                }
                finally
                {
                    try { proc.StandardInput.Close(); } catch { }
                }
            });
        }

        Task stdoutTask = null;
        Task stderrTask = null;

        if (!inheritOut)
        {
            stdoutTask = Task.Run(() =>
            {
                try
                {
                    string line;
                    while ((line = proc.StandardOutput.ReadLine()) != null)
                    {
                        output.WriteLine(line);
                    }
                    output.Flush();
                }
                catch
                {
                    // ignore output pump failures
                }
            });
        }

        if (!inheritErr)
        {
            stderrTask = Task.Run(() =>
            {
                try
                {
                    string line;
                    while ((line = proc.StandardError.ReadLine()) != null)
                    {
                        error.WriteLine(line);
                    }
                    error.Flush();
                }
                catch
                {
                    // ignore stderr pump failures
                }
            });
        }

        proc.WaitForExit();

        if (stdoutTask != null)
            stdoutTask.Wait();

        if (stderrTask != null)
            stderrTask.Wait();

        return proc.ExitCode;
    }
}

public class BlockingCollectionReader : TextReader
{
    private readonly BlockingCollection<string> col;
    private string current;
    private int pos;

    public BlockingCollectionReader(BlockingCollection<string> collection)
    {
        col = collection;
    }

    public override int Read()
    {
        if (current == null || pos >= current.Length)
        {
            current = ReadLine();
            pos = 0;

            if (current == null)
                return -1;
        }

        return current[pos++];
    }

    public override string ReadLine()
    {
        try
        {
            if (col.TryTake(out string s, Timeout.Infinite))
                return s;

            return null;
        }
        catch (InvalidOperationException)
        {
            return null;
        }
    }
}

public class BlockingCollectionWriter : TextWriter
{
    private readonly BlockingCollection<string> col;
    private readonly StringBuilder pending = new();
    private bool completed;

    public BlockingCollectionWriter(BlockingCollection<string> collection)
    {
        col = collection;
    }

    public override Encoding Encoding => Encoding.UTF8;

    public override void Write(char value)
    {
        pending.Append(value);
    }

    public override void Write(string value)
    {
        if (value != null)
            pending.Append(value);
    }

    public override void WriteLine()
    {
        AddLine("");
    }

    public override void WriteLine(string value)
    {
        AddLine(value ?? "");
    }

    private void AddLine(string line)
    {
        pending.Append(line);
        TryAdd(pending.ToString());
        pending.Clear();
    }

    private void TryAdd(string line)
    {
        if (completed) return;

        try
        {
            col.Add(line ?? "");
        }
        catch (InvalidOperationException)
        {
            completed = true;
        }
    }

    public override void Flush()
    {
        if (pending.Length > 0)
        {
            TryAdd(pending.ToString());
            pending.Clear();
        }
    }

    protected override void Dispose(bool disposing)
    {
        if (disposing && !completed)
        {
            Flush();
            completed = true;

            try
            {
                col.CompleteAdding();
            }
            catch
            {
                // ignore
            }
        }

        base.Dispose(disposing);
    }
}

public interface IJetOSApi
{
    string GetInfo();
    void Shutdown();
    void Restart();
    void ShowSettings();
}

public class WindowsJetOSApi : IJetOSApi
{
    public string GetInfo()
    {
        return
            "JetOS API (Windows simulation)\n" +
            $"Machine: {Environment.MachineName}\n" +
            $"OS: {Environment.OSVersion.VersionString}\n" +
            $"User: {Environment.UserName}";
    }

    public void Shutdown()
    {
        if (Environment.GetEnvironmentVariable("JASH_ALLOW_POWER") == "1")
        {
            try
            {
                Process.Start(new ProcessStartInfo
                {
                    FileName = "shutdown.exe",
                    Arguments = "/s /t 0",
                    UseShellExecute = false,
                    CreateNoWindow = true
                });
                return;
            }
            catch
            {
                // fall through to simulation
            }
        }

        Console.Error.WriteLine("JetOS shutdown simulated. Set JASH_ALLOW_POWER=1 to enable real shutdown.");
    }

    public void Restart()
    {
        if (Environment.GetEnvironmentVariable("JASH_ALLOW_POWER") == "1")
        {
            try
            {
                Process.Start(new ProcessStartInfo
                {
                    FileName = "shutdown.exe",
                    Arguments = "/r /t 0",
                    UseShellExecute = false,
                    CreateNoWindow = true
                });
                return;
            }
            catch
            {
                // fall through to simulation
            }
        }

        Console.Error.WriteLine("JetOS restart simulated. Set JASH_ALLOW_POWER=1 to enable real restart.");
    }

    public void ShowSettings()
    {
        Console.WriteLine("JetOS settings (simulated)");
        Console.WriteLine("theme = dark");
        Console.WriteLine("shell = jash");
        Console.WriteLine("update-channel = stable");
    }
}

public static class Builtins
{
    public static void RegisterAll(Runtime rt, ShellEngine shell, IJetOSApi jet)
    {
        // Language-native functions
        rt.NativeFunctions["print"] = args =>
        {
            rt.StdOut.WriteLine(string.Join(" ", args.Select(Values.ToStringValue)));
            return null;
        };

        rt.NativeFunctions["tostring"] = args =>
            args.Count == 0 ? "nil" : Values.ToStringValue(args[0]);

        rt.NativeFunctions["tonumber"] = args =>
        {
            if (args.Count == 0) return null;
            if (Values.TryToNumber(args[0], out double d)) return d;
            return null;
        };

        rt.NativeFunctions["len"] = args =>
        {
            if (args.Count == 0) return 0.0;
            if (args[0] is string s) return (double)s.Length;
            return 0.0;
        };

        rt.NativeFunctions["type"] = args =>
            args.Count == 0 ? Values.TypeName(null) : Values.TypeName(args[0]);

        rt.NativeFunctions["exit_code"] = _ => (double)rt.LastExitCode;
        rt.NativeFunctions["cwd"] = _ => rt.Cwd;

        rt.NativeFunctions["env"] = args =>
        {
            if (args.Count == 0)
            {
                return string.Join("\n", rt.EnvVars.OrderBy(kv => kv.Key).Select(kv => $"{kv.Key}={kv.Value}"));
            }

            string name = Values.ToStringValue(args[0]);
            return rt.EnvVars.TryGetValue(name, out string v) ? v : null;
        };

        rt.NativeFunctions["input"] = args =>
        {
            if (args.Count > 0)
            {
                rt.StdOut.Write(Values.ToStringValue(args[0]));
                rt.StdOut.Flush();
            }

            return rt.StdIn.ReadLine();
        };

        // Shell built-in commands
        shell.Register("help", Help);
        shell.Register("echo", Echo);
        shell.Register("print", Echo);

        shell.Register("ls", Ls);
        shell.Register("dir", Ls);

        shell.Register("cd", Cd);
        shell.Register("pwd", Pwd);

        shell.Register("clear", Clear);
        shell.Register("cls", Clear);

        shell.Register("mkdir", Mkdir);
        shell.Register("md", Mkdir);

        shell.Register("copy", Copy);
        shell.Register("cp", Copy);

        shell.Register("move", Move);
        shell.Register("mv", Move);

        shell.Register("delete", Delete);
        shell.Register("del", Delete);
        shell.Register("rm", Delete);

        shell.Register("cat", Cat);
        shell.Register("type", Cat);

        shell.Register("filter", Filter);
        shell.Register("save", Save);

        shell.Register("env", Env);
        shell.Register("export", Export);

        shell.Register("exit", Exit);
        shell.Register("history", History);

        shell.Register("true", True);
        shell.Register("false", False);

        shell.Register("which", Which);
        shell.Register("run", Run);
        shell.Register("jet", Jet);
    }

    private static int Help(CommandContext ctx)
    {
        ctx.Out.WriteLine("Jash — Bash인데 ㅈㄴ 쉽다!");
        ctx.Out.WriteLine();
        ctx.Out.WriteLine("언어 예:");
        ctx.Out.WriteLine("  name = \"JetOS\"");
        ctx.Out.WriteLine("  print(name)");
        ctx.Out.WriteLine("  if count > 5 then print(\"big\") end");
        ctx.Out.WriteLine("  for i = 1, 10 do print(i) end");
        ctx.Out.WriteLine();
        ctx.Out.WriteLine("Shell 예:");
        ctx.Out.WriteLine("  ls | filter .txt");
        ctx.Out.WriteLine("  cat log.txt | filter error | save result.txt");
        ctx.Out.WriteLine("  cd games");
        ctx.Out.WriteLine("  run game.exe");
        ctx.Out.WriteLine();
        ctx.Out.WriteLine("내장 명령:");
        ctx.Out.WriteLine("  ls dir cd pwd clear cls mkdir copy move delete cat filter save");
        ctx.Out.WriteLine("  echo print env export exit history which run true false jet help");
        return 0;
    }

    private static int Echo(CommandContext ctx)
    {
        ctx.Out.WriteLine(string.Join(" ", ctx.Args));
        return 0;
    }

    private static int Ls(CommandContext ctx)
    {
        var rt = ctx.Runtime;
        var shell = rt.Shell;
        var results = new List<string>();

        try
        {
            if (ctx.Args.Count == 0)
            {
                results.AddRange(Directory.EnumerateFileSystemEntries(rt.Cwd).OrderBy(x => x));
            }
            else
            {
                foreach (var arg in ctx.Args)
                {
                    if (arg == "-l" || arg == "-a") continue;

                    string full = shell.ResolvePath(arg);

                    if (arg.Contains('*') || arg.Contains('?'))
                    {
                        results.AddRange(shell.ExpandPatterns(arg));
                    }
                    else if (Directory.Exists(full))
                    {
                        results.AddRange(Directory.EnumerateFileSystemEntries(full).OrderBy(x => x));
                    }
                    else if (File.Exists(full))
                    {
                        results.Add(full);
                    }
                    else
                    {
                        ctx.Err.WriteLine($"jash: cannot access '{arg}': No such file or directory");
                        return 1;
                    }
                }
            }

            foreach (var p in results.Distinct(StringComparer.OrdinalIgnoreCase).OrderBy(x => x))
            {
                string name = Path.GetFileName(p);
                if (Directory.Exists(p)) name += "/";
                ctx.Out.WriteLine(name);
            }

            return 0;
        }
        catch (Exception e)
        {
            ctx.Err.WriteLine($"jash: ls failed: {e.Message}");
            return 1;
        }
    }

    private static int Cd(CommandContext ctx)
    {
        var rt = ctx.Runtime;

        try
        {
            string target;

            if (ctx.Args.Count == 0)
            {
                target = rt.EnvVars.TryGetValue("USERPROFILE", out string home)
                    ? home
                    : Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            }
            else
            {
                target = ctx.Runtime.Shell.ResolvePath(ctx.Args[0]);
            }

            if (!Directory.Exists(target))
            {
                ctx.Err.WriteLine($"jash: cd: no such directory: {ctx.Args.Count == 0 ? "~" : ctx.Args[0]}");
                return 1;
            }

            rt.Cwd = Path.GetFullPath(target);
            return 0;
        }
        catch (Exception e)
        {
            ctx.Err.WriteLine($"jash: cd failed: {e.Message}");
            return 1;
        }
    }

    private static int Pwd(CommandContext ctx)
    {
        ctx.Out.WriteLine(ctx.Runtime.Cwd);
        return 0;
    }

    private static int Clear(CommandContext ctx)
    {
        try
        {
            if (ReferenceEquals(ctx.Out, ctx.Runtime.StdOut) && ReferenceEquals(ctx.Out, Console.Out))
            {
                Console.Clear();
            }
            else
            {
                ctx.Out.Write("\x1b[2J\x1b[H");
            }

            return 0;
        }
        catch
        {
            return 0;
        }
    }

    private static int Mkdir(CommandContext ctx)
    {
        if (ctx.Args.Count == 0)
        {
            ctx.Err.WriteLine("jash: mkdir: missing directory name");
            return 1;
        }

        try
        {
            foreach (var arg in ctx.Args)
            {
                string path = ctx.Runtime.Shell.ResolvePath(arg);
                Directory.CreateDirectory(path);
            }

            return 0;
        }
        catch (Exception e)
        {
            ctx.Err.WriteLine($"jash: mkdir failed: {e.Message}");
            return 1;
        }
    }

    private static int Copy(CommandContext ctx)
    {
        if (ctx.Args.Count < 2)
        {
            ctx.Err.WriteLine("jash: copy: usage: copy source... dest");
            return 1;
        }

        try
        {
            string destArg = ctx.Args[^1];
            string destPath = ctx.Runtime.Shell.ResolvePath(destArg);

            var sources = new List<string>();

            for (int i = 0; i < ctx.Args.Count - 1; i++)
            {
                var matches = ctx.Runtime.Shell.ExpandPatterns(ctx.Args[i], filesOnly: true);
                if (matches.Count == 0)
                {
                    ctx.Err.WriteLine($"jash: copy: no match: {ctx.Args[i]}");
                    return 1;
                }

                sources.AddRange(matches.Where(File.Exists));
            }

            if (sources.Count == 0)
            {
                ctx.Err.WriteLine("jash: copy: no source files");
                return 1;
            }

            bool destIsDir =
                Directory.Exists(destPath) ||
                destArg.EndsWith('/') ||
                destArg.EndsWith('\\') ||
                sources.Count > 1;

            if (destIsDir)
                Directory.CreateDirectory(destPath);

            foreach (var source in sources)
            {
                string target = destIsDir
                    ? Path.Combine(destPath, Path.GetFileName(source))
                    : destPath;

                File.Copy(source, target, true);
            }

            return 0;
        }
        catch (Exception e)
        {
            ctx.Err.WriteLine($"jash: copy failed: {e.Message}");
            return 1;
        }
    }

    private static int Move(CommandContext ctx)
    {
        if (ctx.Args.Count < 2)
        {
            ctx.Err.WriteLine("jash: move: usage: move source... dest");
            return 1;
        }

        try
        {
            string destArg = ctx.Args[^1];
            string destPath = ctx.Runtime.Shell.ResolvePath(destArg);

            var sources = new List<string>();

            for (int i = 0; i < ctx.Args.Count - 1; i++)
            {
                var matches = ctx.Runtime.Shell.ExpandPatterns(ctx.Args[i], filesOnly: false);
                if (matches.Count == 0)
                {
                    ctx.Err.WriteLine($"jash: move: no match: {ctx.Args[i]}");
                    return 1;
                }

                sources.AddRange(matches.Where(p => File.Exists(p) || Directory.Exists(p)));
            }

            if (sources.Count == 0)
            {
                ctx.Err.WriteLine("jash: move: no source files or directories");
                return 1;
            }

            bool destIsDir =
                Directory.Exists(destPath) ||
                destArg.EndsWith('/') ||
                destArg.EndsWith('\\') ||
                sources.Count > 1;

            if (destIsDir)
                Directory.CreateDirectory(destPath);

            foreach (var source in sources)
            {
                if (Directory.Exists(source))
                {
                    string target = destIsDir
                        ? Path.Combine(destPath, Path.GetFileName(source.TrimEnd('/', '\\')))
                        : destPath;

                    Directory.Move(source, target);
                }
                else
                {
                    string target = destIsDir
                        ? Path.Combine(destPath, Path.GetFileName(source))
                        : destPath;

                    File.Move(source, target, true);
                }
            }

            return 0;
        }
        catch (Exception e)
        {
            ctx.Err.WriteLine($"jash: move failed: {e.Message}");
            return 1;
        }
    }

    private static int Delete(CommandContext ctx)
    {
        bool recursive = false;
        var args = ctx.Args.ToList();

        if (args.Count > 0 && (args[0] == "-r" || args[0] == "/s" || args[0] == "-R"))
        {
            recursive = true;
            args.RemoveAt(0);
        }

        if (args.Count == 0)
        {
            ctx.Err.WriteLine("jash: delete: usage: delete [-r] file-or-pattern...");
            return 1;
        }

        int code = 0;

        foreach (var arg in args)
        {
            try
            {
                bool wildcard = arg.Contains('*') || arg.Contains('?');
                var matches = ctx.Runtime.Shell.ExpandPatterns(arg, filesOnly: false);

                if (wildcard && matches.Count == 0)
                {
                    ctx.Err.WriteLine($"jash: delete: no match: {arg}");
                    code = 1;
                    continue;
                }

                foreach (var path in matches)
                {
                    if (File.Exists(path))
                    {
                        File.Delete(path);
                    }
                    else if (Directory.Exists(path))
                    {
                        Directory.Delete(path, recursive);
                    }
                    else
                    {
                        ctx.Err.WriteLine($"jash: delete: not found: {arg}");
                        code = 1;
                    }
                }
            }
            catch (Exception e)
            {
                ctx.Err.WriteLine($"jash: delete failed: {arg}: {e.Message}");
                code = 1;
            }
        }

        return code;
    }

    private static int Cat(CommandContext ctx)
    {
        try
        {
            if (ctx.Args.Count == 0)
            {
                string line;
                while ((line = ctx.In.ReadLine()) != null)
                    ctx.Out.WriteLine(line);

                return 0;
            }

            foreach (var arg in ctx.Args)
            {
                var matches = ctx.Runtime.Shell.ExpandPatterns(arg, filesOnly: true);
                bool found = false;

                foreach (var file in matches)
                {
                    if (File.Exists(file))
                    {
                        found = true;
                        foreach (var line in File.ReadLines(file, Encoding.UTF8))
                            ctx.Out.WriteLine(line);
                    }
                    else if (Directory.Exists(file))
                    {
                        ctx.Err.WriteLine($"jash: cat: {arg}: is a directory");
                        return 1;
                    }
                }

                if (!found)
                {
                    ctx.Err.WriteLine($"jash: cat: {arg}: no such file");
                    return 1;
                }
            }

            return 0;
        }
        catch (Exception e)
        {
            ctx.Err.WriteLine($"jash: cat failed: {e.Message}");
            return 1;
        }
    }

    private static int Filter(CommandContext ctx)
    {
        if (ctx.Args.Count == 0)
        {
            ctx.Err.WriteLine("jash: filter: usage: filter pattern [files...]");
            return 1;
        }

        string pattern = ctx.Args[0];

        bool Match(string line) =>
            line != null && line.Contains(pattern, StringComparison.OrdinalIgnoreCase);

        try
        {
            if (ctx.Args.Count == 1)
            {
                string line;
                while ((line = ctx.In.ReadLine()) != null)
                {
                    if (Match(line))
                        ctx.Out.WriteLine(line);
                }

                return 0;
            }

            for (int i = 1; i < ctx.Args.Count; i++)
            {
                var matches = ctx.Runtime.Shell.ExpandPatterns(ctx.Args[i], filesOnly: true);
                bool found = false;

                foreach (var file in matches)
                {
                    if (!File.Exists(file)) continue;
                    found = true;

                    foreach (var line in File.ReadLines(file, Encoding.UTF8))
                    {
                        if (Match(line))
                            ctx.Out.WriteLine(line);
                    }
                }

                if (!found)
                {
                    ctx.Err.WriteLine($"jash: filter: {ctx.Args[i]}: no such file");
                    return 1;
                }
            }

            return 0;
        }
        catch (Exception e)
        {
            ctx.Err.WriteLine($"jash: filter failed: {e.Message}");
            return 1;
        }
    }

    private static int Save(CommandContext ctx)
    {
        if (ctx.Args.Count == 0)
        {
            ctx.Err.WriteLine("jash: save: usage: save file");
            return 1;
        }

        try
        {
            string path = ctx.Runtime.Shell.ResolvePath(ctx.Args[0]);
            string dir = Path.GetDirectoryName(path);

            if (!string.IsNullOrEmpty(dir))
                Directory.CreateDirectory(dir);

            using var writer = new StreamWriter(path, false, Encoding.UTF8);

            string line;
            while ((line = ctx.In.ReadLine()) != null)
                writer.WriteLine(line);

            return 0;
        }
        catch (Exception e)
        {
            ctx.Err.WriteLine($"jash: save failed: {e.Message}");
            return 1;
        }
    }

    private static int Env(CommandContext ctx)
    {
        foreach (var kv in ctx.Runtime.EnvVars.OrderBy(kv => kv.Key))
            ctx.Out.WriteLine($"{kv.Key}={kv.Value}");

        return 0;
    }

    private static int Export(CommandContext ctx)
    {
        if (ctx.Args.Count == 0)
            return Env(ctx);

        string name;
        string value;

        if (ctx.Args[0].Contains('='))
        {
            int idx = ctx.Args[0].IndexOf('=');
            name = ctx.Args[0].Substring(0, idx);
            value = ctx.Args[0].Substring(idx + 1);

            if (ctx.Args.Count > 1)
                value += " " + string.Join(" ", ctx.Args.Skip(1));
        }
        else if (ctx.Args.Count >= 3 && ctx.Args[1] == "=")
        {
            name = ctx.Args[0];
            value = string.Join(" ", ctx.Args.Skip(2));
        }
        else if (ctx.Args.Count >= 2)
        {
            name = ctx.Args[0];
            value = string.Join(" ", ctx.Args.Skip(1));
        }
        else
        {
            ctx.Err.WriteLine("jash: export: usage: export NAME value | NAME=value");
            return 1;
        }

        if (string.IsNullOrWhiteSpace(name))
        {
            ctx.Err.WriteLine("jash: export: invalid variable name");
            return 1;
        }

        ctx.Runtime.EnvVars[name] = value ?? "";

        try
        {
            Environment.SetEnvironmentVariable(name, value ?? "");
        }
        catch
        {
            // If current-process env cannot be changed, child env still uses Runtime.EnvVars.
        }

        return 0;
    }

    private static int Exit(CommandContext ctx)
    {
        int code = 0;

        if (ctx.Args.Count > 0 && Values.TryToNumber(ctx.Args[0], out double d))
            code = (int)d;

        ctx.Runtime.ExitRequested = true;
        ctx.Runtime.RequestedExitCode = code;
        return code;
    }

    private static int History(CommandContext ctx)
    {
        var history = ctx.Runtime.History;

        for (int i = 0; i < history.Count; i++)
            ctx.Out.WriteLine($"{i + 1,4}  {history[i]}");

        return 0;
    }

    private static int True(CommandContext ctx) => 0;

    private static int False(CommandContext ctx) => 1;

    private static int Which(CommandContext ctx)
    {
        if (ctx.Args.Count == 0)
        {
            ctx.Err.WriteLine("jash: which: usage: which command");
            return 1;
        }

        string name = ctx.Args[0];

        if (ctx.Runtime.Shell.HasBuiltin(name))
        {
            ctx.Out.WriteLine($"{name}: builtin");
            return 0;
        }

        string resolved = ctx.Runtime.Shell.ResolveExternal(name);

        if (resolved != null)
        {
            ctx.Out.WriteLine(resolved);
            return 0;
        }

        ctx.Err.WriteLine($"jash: which: {name}: not found");
        return 1;
    }

    private static int Run(CommandContext ctx)
    {
        if (ctx.Args.Count == 0)
        {
            ctx.Err.WriteLine("jash: run: usage: run file [args...]");
            return 1;
        }

        try
        {
            string file = ctx.Args[0];
            string resolved = ctx.Runtime.Shell.ResolvePath(file);

            if (File.Exists(resolved))
            {
                file = resolved;
            }
            else
            {
                string external = ctx.Runtime.Shell.ResolveExternal(file);
                if (external != null)
                    file = external;
            }

            var psi = new ProcessStartInfo
            {
                FileName = file,
                UseShellExecute = true,
                WorkingDirectory = ctx.Runtime.Cwd
            };

            if (ctx.Args.Count > 1)
                psi.Arguments = ShellEngine.QuoteArguments(ctx.Args.Skip(1));

            var proc = Process.Start(psi);

            if (proc == null)
                return 0;

            proc.WaitForExit();
            return proc.ExitCode;
        }
        catch (Exception e)
        {
            ctx.Err.WriteLine($"jash: run failed: {e.Message}");
            return 1;
        }
    }

    private static int Jet(CommandContext ctx)
    {
        var jet = ctx.Runtime.JetOS;

        if (jet == null)
        {
            ctx.Err.WriteLine("jash: jet: JetOS API is not available");
            return 1;
        }

        if (ctx.Args.Count == 0)
        {
            ctx.Out.WriteLine("usage: jet [info|shutdown|restart|settings]");
            return 0;
        }

        switch (ctx.Args[0].ToLowerInvariant())
        {
            case "info":
                ctx.Out.WriteLine(jet.GetInfo());
                return 0;

            case "shutdown":
                jet.Shutdown();
                return 0;

            case "restart":
                jet.Restart();
                return 0;

            case "settings":
                jet.ShowSettings();
                return 0;

            default:
                ctx.Err.WriteLine($"jash: jet: unknown subcommand '{ctx.Args[0]}'");
                return 1;
        }
    }
}

public class Interpreter
{
    private class ReturnSignal : Exception
    {
        public object Value;
        public ReturnSignal(object value) => Value = value;
    }

    private class BreakSignal : Exception
    {
    }

    private readonly Runtime rt;

    public Interpreter(Runtime runtime)
    {
        rt = runtime;
    }

    public void Execute(AstProgram program)
    {
        try
        {
            ExecBlock(program.Statements);
        }
        catch (ReturnSignal)
        {
            // top-level return ends script
        }
        catch (BreakSignal)
        {
            // ignore top-level break
        }
    }

    private void ExecBlock(List<Stmt> statements)
    {
        foreach (var stmt in statements)
        {
            if (rt.ExitRequested)
                break;

            ExecStmt(stmt);
        }
    }

    private void ExecStmt(Stmt stmt)
    {
        switch (stmt)
        {
            case AssignStmt assign:
                rt.Vars.Set(assign.Name, Eval(assign.Value));
                break;

            case ExprStmt exprStmt:
                Eval(exprStmt.Expression);
                break;

            case IfStmt ifStmt:
                ExecIf(ifStmt);
                break;

            case WhileStmt whileStmt:
                ExecWhile(whileStmt);
                break;

            case NumericForStmt forStmt:
                ExecNumericFor(forStmt);
                break;

            case ForInStmt forIn:
                ExecForIn(forIn);
                break;

            case FunctionStmt func:
                rt.Functions[func.Name] = new JashFunction
                {
                    Name = func.Name,
                    Params = func.Params,
                    Body = func.Body
                };
                break;

            case ReturnStmt ret:
                object value = ret.Value != null ? Eval(ret.Value) : null;
                throw new ReturnSignal(value);

            case BreakStmt:
                throw new BreakSignal();

            case CommandStmt cmd:
                ExecuteChain(cmd.Chain);
                break;

            default:
                throw new JashError($"unknown statement type: {stmt.GetType().Name}", stmt.Token)
                {
                    File = rt.Source
                };
        }
    }

    private void ExecIf(IfStmt stmt)
    {
        if (Values.Truthy(Eval(stmt.Cond)))
        {
            ExecBlock(stmt.ThenBody);
            return;
        }

        foreach (var elseif in stmt.ElseIfs)
        {
            if (Values.Truthy(Eval(elseif.Cond)))
            {
                ExecBlock(elseif.Body);
                return;
            }
        }

        if (stmt.ElseBody != null)
            ExecBlock(stmt.ElseBody);
    }

    private void ExecWhile(WhileStmt stmt)
    {
        while (Values.Truthy(Eval(stmt.Cond)))
        {
            try
            {
                ExecBlock(stmt.Body);
            }
            catch (BreakSignal)
            {
                break;
            }

            if (rt.ExitRequested)
                break;
        }
    }

    private void ExecNumericFor(NumericForStmt stmt)
    {
        double start = ToNumber(Eval(stmt.Start), stmt.Start.Token);
        double limit = ToNumber(Eval(stmt.Limit), stmt.Limit.Token);
        double step = stmt.Step != null ? ToNumber(Eval(stmt.Step), stmt.Step.Token) : 1.0;

        if (step == 0)
            throw new JashError("for step cannot be zero", stmt.Token) { File = rt.Source };

        for (double i = start; step > 0 ? i <= limit : i >= limit; i += step)
        {
            rt.Vars.Set(stmt.Var, i);

            try
            {
                ExecBlock(stmt.Body);
            }
            catch (BreakSignal)
            {
                break;
            }

            if (rt.ExitRequested)
                break;
        }
    }

    private void ExecForIn(ForInStmt stmt)
    {
        string pattern = Values.ToStringValue(Eval(stmt.Pattern));
        var items = rt.Shell.ExpandForIn(pattern);

        foreach (var item in items)
        {
            rt.Vars.Set(stmt.Var, item);

            try
            {
                ExecBlock(stmt.Body);
            }
            catch (BreakSignal)
            {
                break;
            }

            if (rt.ExitRequested)
                break;
        }
    }

    private int ExecuteChain(CommandChain chain)
    {
        int code = 0;
        bool first = true;

        foreach (var part in chain.Parts)
        {
            bool shouldRun;

            if (first)
            {
                shouldRun = true;
                first = false;
            }
            else if (part.Op == "&&")
            {
                shouldRun = code == 0;
            }
            else if (part.Op == "||")
            {
                shouldRun = code != 0;
            }
            else
            {
                shouldRun = true;
            }

            if (shouldRun)
                code = rt.Shell.ExecutePipeline(part.Pipeline);

            rt.LastExitCode = code;
            rt.Vars.Set("exit_code", (double)code);

            if (rt.ExitRequested)
                break;
        }

        return code;
    }

    private object Eval(Expr expr)
    {
        switch (expr)
        {
            case NumberLiteral n:
                return n.Value;

            case StringLiteral s:
                return s.Value;

            case BoolLiteral b:
                return b.Value;

            case NilLiteral:
                return null;

            case VariableExpr v:
                return GetVariable(v);

            case BinaryExpr binary:
                return EvalBinary(binary);

            case UnaryExpr unary:
                return EvalUnary(unary);

            case CallExpr call:
                return EvalCall(call);

            default:
                throw new JashError($"unknown expression type: {expr.GetType().Name}", expr.Token)
                {
                    File = rt.Source
                };
        }
    }

    private object GetVariable(VariableExpr v)
    {
        if (rt.Vars.TryGet(v.Name, out object value))
            return value;

        if (string.Equals(v.Name, "exit_code", StringComparison.OrdinalIgnoreCase))
            return (double)rt.LastExitCode;

        if (string.Equals(v.Name, "cwd", StringComparison.OrdinalIgnoreCase))
            return rt.Cwd;

        throw new JashError($"unknown variable '{v.Name}'", v.Token)
        {
            File = rt.Source,
            Suggestion = rt.GetSuggestion(v.Name)
        };
    }

    private object EvalBinary(BinaryExpr expr)
    {
        if (expr.Op == "and")
        {
            var left = Eval(expr.Left);
            if (!Values.Truthy(left)) return left;
            return Eval(expr.Right);
        }

        if (expr.Op == "or")
        {
            var left = Eval(expr.Left);
            if (Values.Truthy(left)) return left;
            return Eval(expr.Right);
        }

        var l = Eval(expr.Left);
        var r = Eval(expr.Right);

        switch (expr.Op)
        {
            case "==":
                return Values.ValueEquals(l, r);

            case "!=":
                return !Values.ValueEquals(l, r);

            case "<":
                return Values.Compare(l, r, expr.Token) < 0;

            case "<=":
                return Values.Compare(l, r, expr.Token) <= 0;

            case ">":
                return Values.Compare(l, r, expr.Token) > 0;

            case ">=":
                return Values.Compare(l, r, expr.Token) >= 0;

            case "..":
                return Values.ToStringValue(l) + Values.ToStringValue(r);

            case "+":
            case "-":
            case "*":
            case "/":
            case "%":
                return Arithmetic(expr.Op, l, r, expr.Token);

            default:
                throw new JashError($"unknown operator '{expr.Op}'", expr.Token)
                {
                    File = rt.Source
                };
        }
    }

    private object EvalUnary(UnaryExpr expr)
    {
        var operand = Eval(expr.Operand);

        if (expr.Op == "not")
            return !Values.Truthy(operand);

        if (expr.Op == "-")
        {
            if (!Values.TryToNumber(operand, out double d))
                throw new JashError("attempt to negate a non-number value", expr.Token) { File = rt.Source };

            return -d;
        }

        throw new JashError($"unknown unary operator '{expr.Op}'", expr.Token)
        {
            File = rt.Source
        };
    }

    private object Arithmetic(string op, object l, object r, Token token)
    {
        if (!Values.TryToNumber(l, out double a) || !Values.TryToNumber(r, out double b))
        {
            throw new JashError($"attempt to perform arithmetic on non-number values", token)
            {
                File = rt.Source
            };
        }

        switch (op)
        {
            case "+": return a + b;
            case "-": return a - b;
            case "*": return a * b;

            case "/":
                if (b == 0)
                    throw new JashError("division by zero", token) { File = rt.Source };
                return a / b;

            case "%":
                if (b == 0)
                    throw new JashError("modulo by zero", token) { File = rt.Source };
                return a % b;

            default:
                throw new JashError($"unknown arithmetic operator '{op}'", token)
                {
                    File = rt.Source
                };
        }
    }

    private double ToNumber(object value, Token token)
    {
        if (Values.TryToNumber(value, out double d))
            return d;

        throw new JashError("expected number", token)
        {
            File = rt.Source
        };
    }

    private object EvalCall(CallExpr call)
    {
        if (rt.NativeFunctions.TryGetValue(call.Name, out var native))
        {
            var args = call.Args.Select(Eval).ToList();
            return native(args);
        }

        if (rt.Functions.TryGetValue(call.Name, out var func))
        {
            var args = call.Args.Select(Eval).ToList();
            return CallFunction(func, args, call.Token);
        }

        if (rt.Shell.HasCommandOrExternal(call.Name))
        {
            var seg = new CommandSegment
            {
                Command = call.Name,
                CommandToken = call.Token
            };

            foreach (var argExpr in call.Args)
            {
                object val = Eval(argExpr);
                seg.Args.Add(new CommandArg
                {
                    Text = Values.ToStringValue(val),
                    Quoted = true,
                    Token = argExpr.Token
                });
            }

            var pipeline = new Pipeline();
            pipeline.Segments.Add(seg);

            int code = rt.Shell.ExecutePipeline(pipeline);
            rt.LastExitCode = code;
            rt.Vars.Set("exit_code", (double)code);

            return (double)code;
        }

        throw new JashError($"unknown function or command '{call.Name}'", call.Token)
        {
            File = rt.Source,
            Suggestion = rt.GetSuggestion(call.Name)
        };
    }

    private object CallFunction(JashFunction func, List<object> args, Token callToken)
    {
        rt.Vars.PushScope();

        try
        {
            for (int i = 0; i < func.Params.Count; i++)
            {
                object value = i < args.Count ? args[i] : null;
                rt.Vars.Define(func.Params[i], value);
            }

            ExecBlock(func.Body);
            return null;
        }
        catch (ReturnSignal r)
        {
            return r.Value;
        }
        finally
        {
            rt.Vars.PopScope();
        }
    }
}

public static class StringSimilarity
{
    public static string Best(string input, IEnumerable<string> candidates)
    {
        if (string.IsNullOrEmpty(input) || candidates == null)
            return null;

        string best = null;
        int bestDistance = int.MaxValue;

        foreach (var candidate in candidates)
        {
            if (string.IsNullOrEmpty(candidate)) continue;

            int distance = Levenshtein(input.ToLowerInvariant(), candidate.ToLowerInvariant());

            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = candidate;
            }
        }

        int threshold = input.Length <= 3 ? 1 : input.Length <= 6 ? 2 : 3;

        return bestDistance <= threshold ? best : null;
    }

    private static int Levenshtein(string a, string b)
    {
        if (a == b) return 0;
        if (a.Length == 0) return b.Length;
        if (b.Length == 0) return a.Length;

        var dp = new int[a.Length + 1, b.Length + 1];

        for (int i = 0; i <= a.Length; i++)
            dp[i, 0] = i;

        for (int j = 0; j <= b.Length; j++)
            dp[0, j] = j;

        for (int i = 1; i <= a.Length; i++)
        {
            for (int j = 1; j <= b.Length; j++)
            {
                int cost = a[i - 1] == b[j - 1] ? 0 : 1;

                dp[i, j] = Math.Min(
                    Math.Min(dp[i - 1, j] + 1, dp[i, j - 1] + 1),
                    dp[i - 1, j - 1] + cost);
            }
        }

        return dp[a.Length, b.Length];
    }
}

public class LineEditor
{
    private readonly List<string> history = new();
    private readonly string historyFile;
    private int historyIndex;

    public LineEditor(string historyFile)
    {
        this.historyFile = historyFile;
        LoadHistory();
    }

    public List<string> HistoryList => history;

    public void AddHistory(string line)
    {
        if (string.IsNullOrWhiteSpace(line)) return;

        if (history.Count == 0 || history[^1] != line)
            history.Add(line);

        if (history.Count > 1000)
            history.RemoveAt(0);

        SaveHistory();
    }

    private void LoadHistory()
    {
        try
        {
            if (File.Exists(historyFile))
            {
                history.AddRange(File.ReadAllLines(historyFile)
                    .Where(l => !string.IsNullOrWhiteSpace(l)));
            }
        }
        catch
        {
            // ignore history load errors
        }
    }

    private void SaveHistory()
    {
        try
        {
            File.WriteAllLines(historyFile, history.TakeLast(500));
        }
        catch
        {
            // ignore history save errors
        }
    }

    public string ReadLine(string prompt, string cwd, IEnumerable<string> commandNames)
    {
        string buffer = "";
        int cursor = 0;
        int lastLen = prompt.Length;

        historyIndex = history.Count;
        Console.Write(prompt);

        while (true)
        {
            ConsoleKeyInfo key;

            try
            {
                key = Console.ReadKey(true);
            }
            catch
            {
                return Console.ReadLine();
            }

            if (key.Key == ConsoleKey.Enter)
            {
                Console.WriteLine();
                return buffer;
            }

            if (key.Key == ConsoleKey.D && key.Modifiers == ConsoleModifiers.Control && buffer.Length == 0)
            {
                Console.WriteLine();
                return null;
            }

            if (key.Key == ConsoleKey.C && key.Modifiers == ConsoleModifiers.Control)
            {
                Console.WriteLine();
                buffer = "";
                cursor = 0;
                Render(prompt, buffer, cursor, ref lastLen);
                continue;
            }

            if (key.Key == ConsoleKey.Backspace)
            {
                if (cursor > 0)
                {
                    buffer = buffer.Remove(cursor - 1, 1);
                    cursor--;
                }
            }
            else if (key.Key == ConsoleKey.Delete)
            {
                if (cursor < buffer.Length)
                    buffer = buffer.Remove(cursor, 1);
            }
            else if (key.Key == ConsoleKey.LeftArrow)
            {
                if (cursor > 0) cursor--;
            }
            else if (key.Key == ConsoleKey.RightArrow)
            {
                if (cursor < buffer.Length) cursor++;
            }
            else if (key.Key == ConsoleKey.Home)
            {
                cursor = 0;
            }
            else if (key.Key == ConsoleKey.End)
            {
                cursor = buffer.Length;
            }
            else if (key.Key == ConsoleKey.UpArrow)
            {
                if (historyIndex > 0)
                {
                    historyIndex--;
                    buffer = history[historyIndex];
                    cursor = buffer.Length;
                }
            }
            else if (key.Key == ConsoleKey.DownArrow)
            {
                if (historyIndex < history.Count)
                {
                    historyIndex++;
                    buffer = historyIndex == history.Count ? "" : history[historyIndex];
                    cursor = buffer.Length;
                }
            }
            else if (key.Key == ConsoleKey.Tab)
            {
                var completion = GetCompletion(buffer, cursor, cwd, commandNames);

                if (completion.Candidates.Count == 1)
                {
                    InsertCompletion(completion, ref buffer, ref cursor);
                }
                else if (completion.Candidates.Count > 1)
                {
                    string common = CommonPrefix(completion.Candidates);

                    if (common.Length > completion.Prefix.Length)
                    {
                        buffer = buffer.Substring(0, completion.TokenStart) + common + buffer.Substring(cursor);
                        cursor = completion.TokenStart + common.Length;
                    }
                    else
                    {
                        PrintCandidates(completion.Candidates);
                        lastLen = 0;
                    }
                }
            }
            else if (!char.IsControl(key.KeyChar))
            {
                buffer = buffer.Insert(cursor, key.KeyChar.ToString());
                cursor++;
            }

            Render(prompt, buffer, cursor, ref lastLen);
        }
    }

    private void InsertCompletion(Completion completion, ref string buffer, ref int cursor)
    {
        string text = completion.Candidates[0];

        if (completion.FirstToken && !text.EndsWith(' '))
            text += " ";
        else if (!completion.FirstToken && !text.EndsWith('\\') && !text.EndsWith('"'))
            text += " ";

        buffer = buffer.Substring(0, completion.TokenStart) + text + buffer.Substring(cursor);
        cursor = completion.TokenStart + text.Length;
    }

    private void PrintCandidates(List<string> candidates)
    {
        Console.WriteLine();
        Console.WriteLine(string.Join("  ", candidates.Take(50)));
    }

    private static string CommonPrefix(List<string> values)
    {
        if (values.Count == 0) return "";

        string first = values[0];
        int len = first.Length;

        foreach (var value in values)
        {
            len = Math.Min(len, value.Length);

            for (int i = 0; i < len; i++)
            {
                if (first[i] != value[i])
                {
                    len = i;
                    break;
                }
            }
        }

        return first.Substring(0, len);
    }

    private Completion GetCompletion(string buffer, int cursor, string cwd, IEnumerable<string> commandNames)
    {
        string before = buffer.Substring(0, cursor);

        char[] separators = { ' ', '\t', '|', '>', '<', '&', ';', '(', ')', '"', '\'' };
        int tokenStart = before.LastIndexOfAny(separators) + 1;

        string prefix = before.Substring(tokenStart);
        bool firstToken = string.IsNullOrWhiteSpace(before.Substring(0, tokenStart));

        var completion = new Completion
        {
            TokenStart = tokenStart,
            Prefix = prefix,
            FirstToken = firstToken,
            Candidates = new List<string>()
        };

        if (firstToken)
        {
            completion.Candidates = commandNames
                .Where(c => c.StartsWith(prefix, StringComparison.OrdinalIgnoreCase))
                .OrderBy(c => c)
                .ToList();
        }
        else
        {
            completion.Candidates = CompleteFiles(prefix, cwd);
        }

        return completion;
    }

    private List<string> CompleteFiles(string prefix, string cwd)
    {
        try
        {
            string normalized = prefix.Replace('/', Path.DirectorySeparatorChar);

            if (normalized.StartsWith("~"))
            {
                string home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
                normalized = Path.Combine(home, normalized.Substring(1).TrimStart('\\', '/'));
            }

            string dirPart = null;

            try
            {
                dirPart = Path.GetDirectoryName(normalized);
            }
            catch
            {
                return new List<string>();
            }

            string filePart = Path.GetFileName(normalized);

            if (normalized.EndsWith("\\") || normalized.EndsWith("/"))
                filePart = "";

            string dirPath;

            if (string.IsNullOrEmpty(dirPart))
                dirPath = cwd;
            else if (Path.IsPathRooted(dirPart))
                dirPath = dirPart;
            else
                dirPath = Path.Combine(cwd, dirPart);

            if (!Directory.Exists(dirPath))
                return new List<string>();

            string search = string.IsNullOrEmpty(filePart) ? "*" : filePart + "*";

            return Directory.EnumerateFileSystemEntries(dirPath, search, SearchOption.TopDirectoryOnly)
                .OrderBy(x => x)
                .Select(entry =>
                {
                    string display;

                    if (string.IsNullOrEmpty(dirPart))
                        display = Path.GetFileName(entry);
                    else
                        display = Path.Combine(dirPart, Path.GetFileName(entry));

                    if (Directory.Exists(entry))
                        display += "\\";

                    return display;
                })
                .ToList();
        }
        catch
        {
            return new List<string>();
        }
    }

    private static void Render(string prompt, string buffer, int cursor, ref int lastLen)
    {
        try
        {
            Console.CursorLeft = 0;
        }
        catch
        {
            // ignore
        }

        Console.Write(prompt);
        Console.Write(buffer);

        int currentLen = prompt.Length + buffer.Length;

        if (currentLen < lastLen)
            Console.Write(new string(' ', lastLen - currentLen));

        lastLen = currentLen;

        try
        {
            int target = prompt.Length + cursor;
            int width = Console.BufferWidth;

            if (width > 0)
                Console.CursorLeft = Math.Min(target, width - 1);
        }
        catch
        {
            // ignore cursor positioning errors
        }
    }

    private class Completion
    {
        public int TokenStart;
        public string Prefix;
        public bool FirstToken;
        public List<string> Candidates = new();
    }
}

public static class Program
{
    [STAThread]
    public static int Main(string[] args)
    {
        if (!OperatingSystem.IsWindows())
        {
            Console.Error.WriteLine("Jash MVP is Windows-only.");
            return 1;
        }

        try
        {
            Console.OutputEncoding = Encoding.UTF8;
            Console.InputEncoding = Encoding.UTF8;
        }
        catch
        {
            // Console may be redirected.
        }

        if (args.Length > 0 && args[0] == "--version")
        {
            Console.WriteLine("Jash 0.1.0 (Windows MVP)");
            return 0;
        }

        var runtime = new Runtime(new WindowsJetOSApi());

        if (args.Length > 0)
            return RunFile(runtime, args[0]);

        return RunRepl(runtime);
    }

    private static int RunFile(Runtime rt, string file)
    {
        try
        {
            string fullPath = Path.GetFullPath(file);

            if (!File.Exists(fullPath))
            {
                Console.Error.WriteLine($"Jash Error:\n{fullPath}: file not found");
                return 1;
            }

            rt.Source = fullPath;

            string source = File.ReadAllText(fullPath);
            var program = Parser.Parse(source, fullPath);

            new Interpreter(rt).Execute(program);

            return rt.ExitRequested ? rt.RequestedExitCode : rt.LastExitCode;
        }
        catch (JashError e)
        {
            Console.Error.WriteLine(e.Format());
            return 1;
        }
        catch (Exception e)
        {
            Console.Error.WriteLine($"Jash Error:\n{file}: {e.Message}");
            return 1;
        }
    }

    private static int RunRepl(Runtime rt)
    {
        rt.Source = "<stdin>";

        Console.WriteLine("Jash — Bash인데 ㅈㄴ 쉽다!");
        Console.WriteLine("help를 입력하면 사용법을 볼 수 있습니다. 종료는 exit 입니다.");

        if (Console.IsInputRedirected)
        {
            string line;

            while ((line = Console.ReadLine()) != null)
            {
                if (string.IsNullOrWhiteSpace(line))
                    continue;

                rt.History.Add(line);

                try
                {
                    var program = Parser.Parse(line, "<stdin>");
                    new Interpreter(rt).Execute(program);
                }
                catch (JashError e) when (e.Incomplete)
                {
                    Console.Error.WriteLine("Jash Error: incomplete input");
                }
                catch (JashError e)
                {
                    Console.Error.WriteLine(e.Format());
                }

                if (rt.ExitRequested)
                    break;
            }

            return rt.RequestedExitCode;
        }

        string historyPath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
            ".jash_history");

        var editor = new LineEditor(historyPath);
        rt.History = editor.HistoryList;

        string buffer = null;

        while (!rt.ExitRequested)
        {
            string prompt = buffer == null ? "Jash> " : "  >>> ";

            IEnumerable<string> names = rt.Shell.CommandNames
                .Concat(rt.NativeFunctions.Keys)
                .Concat(rt.Functions.Keys)
                .Distinct(StringComparer.OrdinalIgnoreCase)
                .OrderBy(x => x);

            string line;

            try
            {
                line = editor.ReadLine(prompt, rt.Cwd, names);
            }
            catch
            {
                break;
            }

            if (line == null)
                break;

            if (buffer == null)
            {
                if (string.IsNullOrWhiteSpace(line))
                    continue;

                editor.AddHistory(line);
                buffer = line;
            }
            else
            {
                if (!string.IsNullOrWhiteSpace(line))
                    editor.AddHistory(line);

                buffer += "\n" + line;
            }

            try
            {
                var program = Parser.Parse(buffer, "<stdin>");
                buffer = null;

                new Interpreter(rt).Execute(program);
            }
            catch (JashError e) when (e.Incomplete)
            {
                continue;
            }
            catch (JashError e)
            {
                Console.Error.WriteLine(e.Format());
                buffer = null;
            }
            catch (Exception e)
            {
                Console.Error.WriteLine($"Jash Error:\n{e.Message}");
                buffer = null;
            }
        }

        return rt.RequestedExitCode;
    }
}
```
