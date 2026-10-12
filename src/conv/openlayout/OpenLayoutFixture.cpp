/* Synthetic source document for OpenLayout exporter regression tests.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. https://mozilla.org/MPL/2.0/ */
#include <iostream>
#include "OpenLayoutCollector.h"

using namespace libqxp;

int main(int argc, char **argv)
{
  OpenLayoutOptions options;
  options.unit = argc > 1 && std::string(argv[1]) == "mm" ? OpenLayoutUnit::MILLIMETER : OpenLayoutUnit::POINT;
  options.linkMasters = argc < 3;
  options.title = "Exporter regression fixture";
  OpenLayoutCollector collector(options);
  QXPDocumentProperties prefs;
  prefs.setAutoLeading(0.25);
  prefs.subscriptHScale = 0.6;
  prefs.subscriptVScale = 0.65;
  prefs.subscriptOffset = -0.2;
  collector.collectDocumentProperties(prefs);

  ColorSpec ink;
  ink.id = 1;
  ink.name = "Brand";
  ink.rgb = Color(20, 40, 60);
  collector.collectColor(ink);
  CharacterStyle bold;
  bold.name = "Bold";
  bold.format.bold = true;
  collector.collectCharacterStyle(bold);
  CharacterStyle regular;
  regular.name = "Regular";
  collector.collectCharacterStyle(regular);
  ParagraphStyle paragraph;
  paragraph.name = "Body";
  paragraph.nextIndex = 1;
  paragraph.charStyleIndex = 1;
  paragraph.paragraphFormat.hj = std::make_shared<HJ>();
  paragraph.paragraphFormat.hj->singleWordJustify = false;
  paragraph.paragraphFormat.ruleAbove = std::make_shared<ParagraphRule>();
  paragraph.paragraphFormat.ruleAbove->offset = 3;
  TabStop tab;
  tab.position = 72;
  tab.type = TabStopType::ALIGN;
  tab.alignChar = ",";
  tab.fillChar = ".";
  paragraph.paragraphFormat.tabStops.push_back(tab);
  collector.collectParagraphStyle(paragraph);

  auto text = std::make_shared<Text>();
  text->text = "Ab\x05" "C";
  auto format = std::make_shared<ParagraphFormat>(paragraph.paragraphFormat);
  format->styleIndex = 1;
  text->paragraphs.emplace_back(format, 0, 4);
  auto run = std::make_shared<CharFormat>(regular.format);
  run->styleIndex = 2;
  run->subscript = true;
  text->charFormats.emplace_back(run, 0, 4);
  collector.collectText(text, 43);

  PageSettings size;
  size.offset = Rect(0, 612, 792, 0);
  Page master;
  master.isMaster = true;
  master.pageSettings.push_back(size);
  collector.startPage(master);
  LineStyle dash({0.6, 0.4}, false, 72, LineCapType::ROUND, LineJoinType::BEVEL);
  auto box = std::make_shared<Box>();
  box->boxType = BoxType::RECTANGLE;
  box->boundingBox = Rect(10, 100, 50, 10);
  box->frame.color = Color(0, 0, 0);
  box->frame.lineStyle = &dash;
  collector.collectBox(box);
  auto frame = std::make_shared<TextBox>();
  frame->boundingBox = Rect(100, 300, 200, 10);
  frame->text = text;
  collector.collectTextBox(frame);

  Page page;
  size.masterIndex = 1;
  page.pageSettings.push_back(size);
  collector.startPage(page);
  auto localBox = std::make_shared<Box>(*box);
  localBox->frame.lineStyle = nullptr; // a solid copy must clear the master class
  collector.collectBox(localBox);
  auto linked = std::make_shared<TextBox>(*frame);
  linked->linkSettings.linkId = 43;
  collector.collectTextBox(linked);
  auto second = std::make_shared<TextBox>(*linked);
  second->boundingBox = Rect(210, 300, 310, 10);
  second->linkSettings.offsetIntoText = 2;
  collector.collectTextBox(second);
  auto image = std::make_shared<PictureBox>();
  image->boundingBox = Rect(320, 300, 400, 10);
  image->hasPicture = true; // source unavailable: emit a missingImage child
  image->fileType = "TIFF";
  image->offsetLeft = -4;
  image->offsetTop = -8;
  image->scaleHor = 0.5;
  image->scaleVert = 0.5;
  collector.collectBox(image);
  auto path = std::make_shared<TextPath>();
  path->boundingBox = Rect(500, 82, 500, 10); // 72pt = 96 CSS pixels
  collector.collectLine(path);
  auto gradient = std::make_shared<Box>();
  gradient->boundingBox = Rect(410, 300, 500, 10);
  Gradient fill;
  fill.type = GradientType::DIAMOND;
  fill.angle = 45;
  fill.color1 = Color(0, 0, 0);
  fill.color2 = Color(255, 255, 255);
  gradient->fill = fill;
  collector.collectBox(gradient);

  std::cout << collector.createDocument().toString() << std::endl;
}
