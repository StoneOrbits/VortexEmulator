#include "BehaviourEditor.h"
#include "Behaviours/BehaviourNode.h"

#include <string>
#include <stdio.h>

#define NODE_W 150
#define NODE_H 80
#define SOCKET_R 4

#pragma optimize("", off)

BehaviourEditor::BehaviourEditor()
{
    m_inst = nullptr;
    m_hwnd = nullptr;
    m_nodeCount = 0;

    m_dragging = false;
    m_dragNode = -1;

    m_linking = false;
    m_linkNode = -1;
    m_linkMouseX = 0;
    m_linkMouseY = 0;
}

bool BehaviourEditor::init(HINSTANCE inst, HWND parent, int x, int y, int w, int h)
{
    m_inst = inst;

    WNDCLASS wc = {0};
    wc.lpfnWndProc = BehaviourEditor::wndproc;
    wc.hInstance = inst;
    wc.lpszClassName = "BehaviourEditorWnd";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    m_hwnd = CreateWindowEx(
        WS_EX_APPWINDOW,
        "BehaviourEditorWnd",
        "Behaviour Node Editor",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        100,
        100,
        900,
        600,
        NULL,
        NULL,
        inst,
        this
    );

    return m_hwnd != NULL;
}

HWND BehaviourEditor::hwnd()
{
    return m_hwnd;
}

void BehaviourEditor::redraw()
{
    InvalidateRect(m_hwnd, NULL, TRUE);
}

int BehaviourEditor::findNodeAt(int x, int y)
{
    for (int i = m_nodeCount - 1; i >= 0; i--) {
        EditorNode &n = m_nodes[i];
        if (x >= n.x && x <= n.x + NODE_W &&
            y >= n.y && y <= n.y + NODE_H)
            return i;
    }
    return -1;
}

int BehaviourEditor::findOutputSocket(int x, int y)
{
    for (int i = 0; i < m_nodeCount; i++) {
        EditorNode &n = m_nodes[i];
        int sx = n.x + NODE_W;
        int sy = n.y + NODE_H / 2;
        int dx = x - sx;
        int dy = y - sy;
        if (dx*dx + dy*dy <= 64)
            return i;
    }
    return -1;
}

void BehaviourEditor::onMouseDown(int x, int y)
{
    int sock = findOutputSocket(x, y);
    if (sock != -1) {
        m_linking = true;
        m_linkNode = sock;
        m_linkMouseX = x;
        m_linkMouseY = y;
        redraw();
        return;
    }

    int node = findNodeAt(x, y);
    if (node != -1) {
        m_dragging = true;
        m_dragNode = node;
        m_dragOffsetX = x - m_nodes[node].x;
        m_dragOffsetY = y - m_nodes[node].y;
    }
}

void BehaviourEditor::onMouseUp(int x, int y)
{
    if (!m_linking) {
        m_dragging = false;
        return;
    }

    BehaviourNode *src = Behaviours::node(m_nodes[m_linkNode].behaviourIndex);

    for (int i = 0; i < m_nodeCount; i++) {
        if (i == m_linkNode) continue; // cannot link to self
        EditorNode &n = m_nodes[i];
        BehaviourNode *bn = Behaviours::node(n.behaviourIndex);
        uint8_t inputs = bn->inputCount();

        for (uint8_t j = 0; j < inputs; j++) {
            int sx = n.x;
            int sy = n.y + 30 + j * 16;
            int dx = x - sx;
            int dy = y - sy;

            if (dx*dx + dy*dy <= 64) {
                bn->addInput(src);
                break;
            }
        }
    }

    m_linking = false;
    redraw();
}

void BehaviourEditor::onMouseMove(int x, int y)
{
    if (m_linking) {
        m_linkMouseX = x;
        m_linkMouseY = y;
        redraw();
        return;
    }

    if (!m_dragging)
        return;

    EditorNode &n = m_nodes[m_dragNode];
    n.x = x - m_dragOffsetX;
    n.y = y - m_dragOffsetY;
    redraw();
}

void BehaviourEditor::showContextMenu(int screenX, int screenY, int clientX, int clientY)
{
    HMENU menu = CreatePopupMenu();
    struct MenuItem {
        uint32_t id;
        Behaviours::NodeType type;
        const char *name;
    };

    MenuItem items[] = {
        {1, Behaviours::NODE_ACCEL_MOTION, "Accel Motion"},
        {2, Behaviours::NODE_ACCEL_NORMALIZED, "Accel Normalized"},
        {3, Behaviours::NODE_ACCEL_CURVED, "Accel Curved"},
        {4, Behaviours::NODE_ACCEL_FILTERED, "Accel Filtered"},
        {5, Behaviours::NODE_ACCEL_DIR_X, "Accel Dir X"},
        {6, Behaviours::NODE_ACCEL_DIR_Y, "Accel Dir Y"},
        {7, Behaviours::NODE_ACCEL_DIR_Z, "Accel Dir Z"},
        {8, Behaviours::NODE_ACCEL_TILT, "Accel Tilt"},
        {9, Behaviours::NODE_ABS, "Abs"},
        {10, Behaviours::NODE_ADD, "Add"},
        {11, Behaviours::NODE_MULTIPLY, "Multiply"},
        {12, Behaviours::NODE_CLAMP, "Clamp"},
        {13, Behaviours::NODE_CURVE, "Curve"},
        {14, Behaviours::NODE_THRESHOLD, "Threshold"},
        {15, Behaviours::NODE_MODE_BLEND, "Mode Blend"}
    };

    for (auto &i : items) AppendMenu(menu, MF_STRING, i.id, i.name);

    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD, screenX, screenY, 0, m_hwnd, NULL);
    DestroyMenu(menu);
    if (cmd == 0) return;

    Behaviours::NodeType type = Behaviours::NODE_ADD;
    for (auto &i : items) if (i.id == (uint32_t)cmd) type = i.type;

    uint8_t idx = Behaviours::create(type);
    if (idx == 255) return;

    EditorNode &n = m_nodes[m_nodeCount++];
    n.behaviourIndex = idx;
    n.x = clientX;
    n.y = clientY;
    redraw();
}

void BehaviourEditor::draw(HDC dc)
{
    RECT rc;
    GetClientRect(m_hwnd, &rc);

    HBRUSH bg = CreateSolidBrush(RGB(35,35,35));
    FillRect(dc, &rc, bg);
    DeleteObject(bg);

    HPEN pen = CreatePen(PS_SOLID, 2, RGB(200,200,200));
    SelectObject(dc, pen);

    // Draw connections
    for (int i=0;i<m_nodeCount;i++) {
        BehaviourNode *bn = Behaviours::node(m_nodes[i].behaviourIndex);
        for (uint8_t j=0;j<bn->inputCount();j++) {
            BehaviourNode *src = bn->input(j);
            if (!src) continue;
            for (int k=0;k<m_nodeCount;k++) {
                if (Behaviours::node(m_nodes[k].behaviourIndex) == src) {
                    int x1 = m_nodes[k].x + NODE_W;
                    int y1 = m_nodes[k].y + NODE_H/2;
                    int x2 = m_nodes[i].x;
                    int y2 = m_nodes[i].y + 30 + j*16;
                    MoveToEx(dc,x1,y1,NULL);
                    LineTo(dc,x2,y2);
                }
            }
        }
    }

    // Draw linking preview line
    if (m_linking && m_linkNode >= 0 && m_linkNode < m_nodeCount) {
        int x1 = m_nodes[m_linkNode].x + NODE_W;
        int y1 = m_nodes[m_linkNode].y + NODE_H/2;
        MoveToEx(dc, x1, y1, NULL);
        LineTo(dc, m_linkMouseX, m_linkMouseY);
    }

    DeleteObject(pen);

    // Draw nodes
    for (uint8_t i=0;i<m_nodeCount;i++) {
        EditorNode &en = m_nodes[i];
        BehaviourNode *bn = Behaviours::node(en.behaviourIndex);

        RECT r = { en.x, en.y, en.x + NODE_W, en.y + NODE_H };
        COLORREF color = nodeColor(bn->type());
        HBRUSH br = CreateSolidBrush(color);
        FillRect(dc, &r, br);
        DeleteObject(br);

        HBRUSH oldBrush = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
        Rectangle(dc, r.left, r.top, r.right, r.bottom);
        SelectObject(dc, oldBrush);

        SetTextColor(dc, RGB(255,255,255));
        SetBkMode(dc, TRANSPARENT);

        const char *name = nodeTypeName(bn->type());
        TextOutA(dc, r.left+6, r.top+4, name, strlen(name));

        char buf[64];
        sprintf_s(buf,sizeof(buf),"p0 %.2f",bn->param0);
        TextOutA(dc,r.left+6,r.top+24,buf,strlen(buf));
        sprintf_s(buf,sizeof(buf),"p1 %.2f",bn->param1);
        TextOutA(dc,r.left+6,r.top+42,buf,strlen(buf));

        // Input sockets
        for (uint8_t j=0;j<bn->inputCount();j++) {
            int sy = r.top + 30 + j*16;
            int sx = r.left - SOCKET_R*2;
            Ellipse(dc,sx,sy,sx+SOCKET_R*2,sy+SOCKET_R*2);
        }

        // Output socket
        int ox = r.right;
        int oy = r.top + NODE_H/2;
        Ellipse(dc, ox, oy-SOCKET_R, ox+SOCKET_R*2, oy+SOCKET_R);
    }
}

LRESULT CALLBACK BehaviourEditor::wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    BehaviourEditor *ed;

    if (msg==WM_NCCREATE) {
        CREATESTRUCT *cs = (CREATESTRUCT*)lParam;
        ed = (BehaviourEditor*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)ed);
        return TRUE;
    }

    ed = (BehaviourEditor*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!ed) return DefWindowProc(hwnd,msg,wParam,lParam);

    switch(msg) {
        case WM_LBUTTONDOWN:
            ed->onMouseDown(LOWORD(lParam),HIWORD(lParam));
            return 0;

        case WM_LBUTTONUP:
            ed->onMouseUp(LOWORD(lParam),HIWORD(lParam));
            return 0;

        case WM_MOUSEMOVE:
            ed->onMouseMove(LOWORD(lParam),HIWORD(lParam));
            return 0;

        case WM_RBUTTONUP: {
            POINT screen; screen.x=LOWORD(lParam); screen.y=HIWORD(lParam);
            ClientToScreen(hwnd,&screen);
            ed->showContextMenu(screen.x, screen.y, LOWORD(lParam), HIWORD(lParam));
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd,&ps);
            ed->draw(dc);
            EndPaint(hwnd,&ps);
            return 0;
        }
    }
    return DefWindowProc(hwnd,msg,wParam,lParam);
}

COLORREF BehaviourEditor::nodeColor(Behaviours::NodeType t)
{
    switch(t){
        case Behaviours::NODE_ACCEL_MOTION:
        case Behaviours::NODE_ACCEL_NORMALIZED:
        case Behaviours::NODE_ACCEL_CURVED:
        case Behaviours::NODE_ACCEL_FILTERED:
        case Behaviours::NODE_ACCEL_DIR_X:
        case Behaviours::NODE_ACCEL_DIR_Y:
        case Behaviours::NODE_ACCEL_DIR_Z:
        case Behaviours::NODE_ACCEL_TILT: return RGB(70,120,200);

        case Behaviours::NODE_ADD:
        case Behaviours::NODE_MULTIPLY:
        case Behaviours::NODE_ABS: return RGB(200,120,70);

        case Behaviours::NODE_CLAMP:
        case Behaviours::NODE_CURVE:
        case Behaviours::NODE_THRESHOLD: return RGB(120,200,120);

        case Behaviours::NODE_MODE_BLEND: return RGB(200,80,200);
    }
    return RGB(80,80,80);
}

const char* BehaviourEditor::nodeTypeName(Behaviours::NodeType t)
{
    switch(t){
        case Behaviours::NODE_ACCEL_MOTION: return "Accel Motion";
        case Behaviours::NODE_ACCEL_NORMALIZED: return "Accel Norm";
        case Behaviours::NODE_ACCEL_CURVED: return "Accel Curve";
        case Behaviours::NODE_ACCEL_FILTERED: return "Accel Filter";
        case Behaviours::NODE_ACCEL_DIR_X: return "Accel X";
        case Behaviours::NODE_ACCEL_DIR_Y: return "Accel Y";
        case Behaviours::NODE_ACCEL_DIR_Z: return "Accel Z";
        case Behaviours::NODE_ACCEL_TILT: return "Tilt";
        case Behaviours::NODE_ABS: return "Abs";
        case Behaviours::NODE_ADD: return "Add";
        case Behaviours::NODE_MULTIPLY: return "Multiply";
        case Behaviours::NODE_CLAMP: return "Clamp";
        case Behaviours::NODE_CURVE: return "Curve";
        case Behaviours::NODE_THRESHOLD: return "Threshold";
        case Behaviours::NODE_MODE_BLEND: return "Blend";
    }
    return "Node";
}