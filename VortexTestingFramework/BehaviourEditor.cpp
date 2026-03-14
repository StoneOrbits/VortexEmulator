#include "BehaviourEditor.h"

#include "Behaviours/BehaviourNode.h"

#include <string>

#define NODE_W 120
#define NODE_H 40

#pragma optimize("", off)

BehaviourEditor::BehaviourEditor()
{
  m_inst = nullptr;
  m_hwnd = nullptr;
  m_nodeCount = 0;

  m_dragging = false;
  m_dragNode = -1;
}

bool BehaviourEditor::init(HINSTANCE inst, HWND parent, int x, int y, int w, int h)
{
  m_inst = inst;

  WNDCLASS wc = { 0 };
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
    800,
    500,
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

    if (x >= n.x &&
      x <= n.x + NODE_W &&
      y >= n.y &&
      y <= n.y + NODE_H) {
      return i;
    }
  }

  return -1;
}

void BehaviourEditor::onMouseDown(int x, int y)
{
  int node = findNodeAt(x, y);

  if (node != -1) {
    m_dragging = true;
    m_dragNode = node;

    m_dragOffsetX = x - m_nodes[node].x;
    m_dragOffsetY = y - m_nodes[node].y;
  }
}

void BehaviourEditor::onMouseUp()
{
  m_dragging = false;
}

void BehaviourEditor::onMouseMove(int x, int y)
{
  if (!m_dragging)
    return;

  EditorNode &n = m_nodes[m_dragNode];

  n.x = x - m_dragOffsetX;
  n.y = y - m_dragOffsetY;

  redraw();
}

void BehaviourEditor::showContextMenu(int x, int y)
{
  HMENU menu = CreatePopupMenu();

  struct MenuItem
  {
    uint32_t id;
    Behaviours::NodeType type;
    const char *name;
  };

  MenuItem items[] =
  {
    {1, Behaviours::NODE_ACCEL_MOTION,       "Accel Motion"},
    {2, Behaviours::NODE_ACCEL_NORMALIZED,   "Accel Normalized"},
    {3, Behaviours::NODE_ACCEL_CURVED,       "Accel Curved"},
    {4, Behaviours::NODE_ACCEL_FILTERED,     "Accel Filtered"},

    {5, Behaviours::NODE_ACCEL_DIR_X,        "Accel Dir X"},
    {6, Behaviours::NODE_ACCEL_DIR_Y,        "Accel Dir Y"},
    {7, Behaviours::NODE_ACCEL_DIR_Z,        "Accel Dir Z"},

    {8, Behaviours::NODE_ACCEL_TILT,         "Accel Tilt"},

    {9,  Behaviours::NODE_ABS,               "Abs"},
    {10, Behaviours::NODE_ADD,               "Add"},
    {11, Behaviours::NODE_MULTIPLY,          "Multiply"},
    {12, Behaviours::NODE_CLAMP,             "Clamp"},
    {13, Behaviours::NODE_CURVE,             "Curve"},
    {14, Behaviours::NODE_THRESHOLD,         "Threshold"},

    {15, Behaviours::NODE_MODE_BLEND,        "Mode Blend"}
  };

  for (auto &i : items)
  {
    AppendMenu(menu, MF_STRING, i.id, i.name);
  }

  int cmd = TrackPopupMenu(
    menu,
    TPM_RETURNCMD,
    x,
    y,
    0,
    m_hwnd,
    NULL
  );

  DestroyMenu(menu);

  if (cmd == 0)
    return;

  Behaviours::NodeType type = Behaviours::NODE_ADD;

  for (auto &i : items)
  {
    if (i.id == (uint32_t)cmd)
    {
      type = i.type;
      break;
    }
  }

  uint8_t idx = Behaviours::create(type);

  if (idx == 255)
    return;

  EditorNode &n = m_nodes[m_nodeCount++];

  n.behaviourIndex = idx;

  POINT pt;
  GetCursorPos(&pt);
  ScreenToClient(m_hwnd, &pt);

  n.x = pt.x;
  n.y = pt.y;

  redraw();
}

void BehaviourEditor::draw(HDC dc)
{
  HBRUSH bg = CreateSolidBrush(RGB(35, 35, 35));
  RECT rc;
  GetClientRect(m_hwnd, &rc);
  FillRect(dc, &rc, bg);
  DeleteObject(bg);

  for (uint8_t i = 0; i < m_nodeCount; i++) {
    EditorNode &en = m_nodes[i];

    RECT r =
    {
        en.x,
        en.y,
        en.x + NODE_W,
        en.y + NODE_H
    };

    HBRUSH br = CreateSolidBrush(RGB(60, 60, 60));
    HBRUSH br = CreateSolidBrush(nodeColor(bn->type()));
    FillRect(dc, &r, br);
    DeleteObject(br);

    Rectangle(dc, r.left, r.top, r.right, r.bottom);

    BehaviourNode *bn = Behaviours::node(en.behaviourIndex);

    std::string name = std::to_string((int)bn->type());

    SetTextColor(dc, RGB(255, 255, 255));
    SetBkMode(dc, TRANSPARENT);

    TextOutA(dc, r.left + 6, r.top + 6, name.c_str(), name.size());
  }
}

LRESULT CALLBACK BehaviourEditor::wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  BehaviourEditor *ed;

  if (msg == WM_NCCREATE) {
    CREATESTRUCT *cs = (CREATESTRUCT *)lParam;
    ed = (BehaviourEditor *)cs->lpCreateParams;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)ed);
    return TRUE;
  }

  ed = (BehaviourEditor *)GetWindowLongPtr(hwnd, GWLP_USERDATA);

  if (!ed)
    return DefWindowProc(hwnd, msg, wParam, lParam);

  switch (msg) {
  case WM_LBUTTONDOWN:
    ed->onMouseDown(LOWORD(lParam), HIWORD(lParam));
    return 0;
  case WM_LBUTTONUP:
    ed->onMouseUp();
    return 0;
  case WM_MOUSEMOVE:
    ed->onMouseMove(LOWORD(lParam), HIWORD(lParam));
    return 0;
  case WM_RBUTTONUP:
  {
    POINT pt;
    GetCursorPos(&pt);
    ed->showContextMenu(pt.x, pt.y);
    return 0;
  }

  case WM_PAINT:
  {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    ed->draw(dc);
    EndPaint(hwnd, &ps);
    return 0;
  }
  }

  return DefWindowProc(hwnd, msg, wParam, lParam);
}

COLORREF nodeColor(Behaviours::NodeType t)
{
    switch (t)
    {
        case Behaviours::NODE_ACCEL_MOTION:
        case Behaviours::NODE_ACCEL_NORMALIZED:
        case Behaviours::NODE_ACCEL_CURVED:
        case Behaviours::NODE_ACCEL_FILTERED:
        case Behaviours::NODE_ACCEL_DIR_X:
        case Behaviours::NODE_ACCEL_DIR_Y:
        case Behaviours::NODE_ACCEL_DIR_Z:
        case Behaviours::NODE_ACCEL_TILT:
            return RGB(70,120,200);

        case Behaviours::NODE_ADD:
        case Behaviours::NODE_MULTIPLY:
        case Behaviours::NODE_ABS:
            return RGB(200,120,70);

        case Behaviours::NODE_CLAMP:
        case Behaviours::NODE_CURVE:
        case Behaviours::NODE_THRESHOLD:
            return RGB(120,200,120);

        case Behaviours::NODE_MODE_BLEND:
            return RGB(200,80,200);
    }

    return RGB(80,80,80);
}

const char *BehaviourEditor::nodeTypeName(Behaviours::NodeType t)
{
    switch (t)
    {
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